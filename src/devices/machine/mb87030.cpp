// license:BSD-3-Clause
// copyright-holders:Sven Schnelle

#include "emu.h"
#include "mb87030.h"

//#define VERBOSE 1
#include "logmacro.h"


DEFINE_DEVICE_TYPE(MB87030, mb87030_device, "mb87030", "Fujitsu MB87030 SCSI controller")

mb87030_device::mb87030_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	mb87030_device(mconfig, MB87030, tag, owner, clock)
{

}

mb87030_device::mb87030_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock) :
	nscsi_device(mconfig, type, tag, owner, clock),
	m_irq_handler(*this),
	m_dreq_handler(*this)
{

}

void mb87030_device::map(address_map &map)
{
	map(0x00, 0x00).rw(FUNC(mb87030_device::bdid_r), FUNC(mb87030_device::bdid_w));
	map(0x01, 0x01).rw(FUNC(mb87030_device::sctl_r), FUNC(mb87030_device::sctl_w));
	map(0x02, 0x02).rw(FUNC(mb87030_device::scmd_r), FUNC(mb87030_device::scmd_w));
	map(0x03, 0x03).rw(FUNC(mb87030_device::tmod_r), FUNC(mb87030_device::tmod_w));
	map(0x04, 0x04).rw(FUNC(mb87030_device::ints_r), FUNC(mb87030_device::ints_w));
	map(0x05, 0x05).rw(FUNC(mb87030_device::psns_r), FUNC(mb87030_device::sdgc_w));
	map(0x06, 0x06).r(FUNC(mb87030_device::ssts_r));
	map(0x07, 0x07).r(FUNC(mb87030_device::serr_r));
	map(0x08, 0x08).rw(FUNC(mb87030_device::pctl_r), FUNC(mb87030_device::pctl_w));
	map(0x09, 0x09).r(FUNC(mb87030_device::mbc_r));
	map(0x0a, 0x0a).rw(FUNC(mb87030_device::dreg_r), FUNC(mb87030_device::dreg_w));
	map(0x0b, 0x0b).rw(FUNC(mb87030_device::temp_r), FUNC(mb87030_device::temp_w));
	map(0x0c, 0x0c).rw(FUNC(mb87030_device::tch_r), FUNC(mb87030_device::tch_w));
	map(0x0d, 0x0d).rw(FUNC(mb87030_device::tcm_r), FUNC(mb87030_device::tcm_w));
	map(0x0e, 0x0e).rw(FUNC(mb87030_device::tcl_r), FUNC(mb87030_device::tcl_w));
	map(0x0f, 0x0f).rw(FUNC(mb87030_device::exbf_r), FUNC(mb87030_device::exbf_w));
}

void mb87030_device::device_reset()
{
	m_bdid = 0;
	m_sctl = SCTL_RESET_AND_DISABLE;
	m_scmd = 0;
	m_tmod = 0;
	m_ints = 0;
	m_sdgc = 0;
	m_ssts = 0;
	m_serr = 0;
	m_pctl = 0;
	m_mbc = 0;
	m_dreg = 0;
	m_temp = 0;
	m_tch = 0;
	m_tcm = 0;
	m_tc = 0;
	m_exbf = 0;
	m_fifo.clear();
	scsi_bus->ctrl_wait(scsi_refid, S_SEL|S_BSY|S_RST, S_ALL);
	update_state(State::Idle, 0);
	scsi_set_ctrl(0, S_ALL);
	scsi_bus->data_w(scsi_refid, 0);
	update_ssts();
	update_ints();
}

auto mb87030_device::get_state_name(State state) const
{
	switch (state) {
	case State::Idle:
		return "Idle";
	case State::ArbitrationWaitBusFree:
		return "ArbitrationWaitBusFree";
	case State::ArbitrationAssertBSY:
		return "ArbitrationAssertBSY";
	case State::ArbitrationWait:
		return "ArbitrationWait";
	case State::ArbitrationAssertSEL:
		return "ArbitrationAssertSEL";
	case State::ArbitrationDeAssertBSY:
		return "ArbitrationDeAssertBSY";
	case State::SelectionWaitBusFree:
		return "SelectionWaitBusFree";
	case State::SelectionWaitBSY:
		return "SelectionWaitBSY";
	case State::SelectionAssertSEL:
		return "SelectionAssertSEL";
	case State::Selection:
		return "Selection";
	case State::TransferWaitReq:
		return "TransferWaitReq";
	case State::TransferSendAck:
		return "TransferSendAck";
	case State::TransferSendData:
		return "TransferSendData";
	case State::TransferSendDataDMAReq:
		return "TransferSendDataDMAReq";
	case State::TransferSendDataDMAResp:
		return "TransferSendDataDMAResp";
	case State::TransferRecvData:
		return "TransferRecvData";
	case State::TransferRecvDataDMAReq:
		return "TransferRecvDataDMAReq";
	case State::TransferRecvDataDMAResp:
		return "TransferRecvDataDMAResp";

	case State::TransferWaitDeassertREQ:
		return "TransferWaitDeassertREQ";
	case State::TransferDeassertACK:
		return "TransferDeassertACK";
	}
	return "Unknown state";
}

void mb87030_device::update_state(mb87030_device::State new_state, int delay, int timeout)
{
	LOG("new state: %s -> %s (delay %d, timeout %d)\n", get_state_name(m_state),
			get_state_name(new_state), delay, timeout);
	m_state = new_state;
	if (delay)
		m_delay_timer->adjust(clocks_to_attotime(delay));
	else
		m_delay_timer->reset();
	if (timeout)
		m_timer->adjust(clocks_to_attotime(timeout));
	else
		m_timer->reset();
}

TIMER_CALLBACK_MEMBER(mb87030_device::timeout)
{
	m_timer->reset();
	step(true);
}

TIMER_CALLBACK_MEMBER(mb87030_device::delay_timeout)
{
	m_delay_timer->reset();
	step(false);
}

void mb87030_device::scsi_command_complete()
{
	m_ints |= INTS_COMMAND_COMPLETE;
	m_ssts &= ~(SSTS_SPC_BUSY|SSTS_XFER_IN_PROGRESS);
	update_ints();
	update_state(State::Idle);
}

void mb87030_device::scsi_disconnect_timeout()
{
	scsi_set_ctrl(0, S_ALL);
	scsi_bus->data_w(scsi_refid, 0);
	m_ints = INTS_SPC_TIMEOUT;
	update_ints();
	update_state(State::Idle);
}

void mb87030_device::scsi_disconnect()
{
	LOG("%s: m_tc %d\n", __FUNCTION__, m_tc);
	m_ssts &= ~(SSTS_INIT_CONNECTED|SSTS_TARG_CONNECTED|SSTS_SPC_BUSY|SSTS_XFER_IN_PROGRESS);
	m_ints = INTS_DISCONNECTED;
	update_ints();
	update_state(State::Idle);
}

void mb87030_device::scsi_set_ctrl(uint32_t value, uint32_t mask)
{
	if (m_sctl & SCTL_DIAG_MODE) {
		m_scsi_ctrl &= ~mask;
		m_scsi_ctrl |= value;
		logerror("update m_scsi_ctrl: %02X%s%s%s%s%s%s\n", m_scsi_ctrl,
				(m_scsi_ctrl & S_REQ) ? " REQ" : "",
				(m_scsi_ctrl & S_ACK) ? " ACK" : "",
				(m_scsi_ctrl & S_BSY) ? " BSY" : "",
				(m_scsi_ctrl & S_MSG) ? " MSG" : "",
				(m_scsi_ctrl & S_CTL) ? " CTL" : "",
				(m_scsi_ctrl & S_INP) ? " INP" : "");
	} else {
		scsi_bus->ctrl_w(scsi_refid, value, mask);
	}
}

uint32_t mb87030_device::scsi_get_ctrl(void)
{
	if (m_sctl & SCTL_DIAG_MODE) {
		uint32_t ret = 0;
		if ((m_sdgc & SDGC_DIAG_IO) || (m_scsi_ctrl & S_INP))
			ret |= S_INP;
		if ((m_sdgc & SDGC_DIAG_CD) || (m_scsi_ctrl & S_CTL))
			ret |= S_CTL;
		if ((m_sdgc & SDGC_DIAG_MSG) || (m_scsi_ctrl & S_MSG))
			ret |= S_MSG;
		if ((m_sdgc & SDGC_DIAG_BSY) || (m_scsi_ctrl & S_BSY))
			ret |= S_BSY;
		if ((m_sdgc & SDGC_DIAG_ACK) || (m_scsi_ctrl & S_ACK))
			ret |= S_ACK;
		if ((m_sdgc & SDGC_DIAG_REQ) || (m_scsi_ctrl & S_REQ))
			ret |= S_REQ;
		if (m_scsi_ctrl & S_SEL)
			ret |= S_SEL;
		if (m_scsi_ctrl & S_ATN)
			ret |= S_ATN;

		return ret;
	} else {
		return scsi_bus->ctrl_r();
	}
}

void mb87030_device::step(bool timeout)
{
	uint32_t ctrl = scsi_get_ctrl();
	uint32_t data = scsi_bus->data_r();

	LOG("%s: %d %s,%s data %02X ctrl %02X TC %d\n", __FUNCTION__,
			scsi_refid, get_state_name(m_state),
			timeout ? " timeout" : "", data,
					ctrl, m_tc);

	if (m_sctl & SCTL_RESET_AND_DISABLE && m_state != State::Idle) {
		scsi_set_ctrl(0, S_ALL);
		update_state(State::Idle);
		return;
	}

	if ((m_ssts & SSTS_INIT_CONNECTED) && !(ctrl & S_BSY)) {
		LOG("SCSI disconnect\n");
		scsi_disconnect();
		scsi_set_ctrl(0, S_ALL);
	}

	switch (m_state) {
	case State::Idle:
		if (ctrl == 0 && (m_pctl & PCTL_BUS_FREE_IE)) {
			m_ints |= INTS_DISCONNECTED;
			update_ints();
		}
		break;

	case State::ArbitrationWaitBusFree:
		if (!(ctrl & (S_BSY|S_SEL)))
			update_state(State::ArbitrationAssertBSY, 1);

		break;
	case State::ArbitrationAssertBSY:
		scsi_set_ctrl(S_BSY, S_BSY);
		scsi_bus->data_w(scsi_refid, (1 << m_bdid));
		update_state(State::ArbitrationWait, 1);
		break;

	case State::ArbitrationWait:
		for (int id = (2 << m_bdid); id <= 0x80; id <<= 1) {
			LOG("check %d\n", id);
			if (data & id) {
				LOG("arbitration lost, winner %d\n", id);
				scsi_disconnect_timeout();
				break;
			}
		}
		LOG("Arbitration won\n");
		update_state(State::ArbitrationAssertSEL, 1);
		break;

	case State::ArbitrationAssertSEL:
		scsi_set_ctrl(S_SEL, S_SEL);
		update_state(State::ArbitrationDeAssertBSY, 1);
		break;

	case State::ArbitrationDeAssertBSY:
		scsi_set_ctrl(0, S_BSY);
		update_state(State::SelectionAssertSEL, 1);
		break;

	case State::SelectionWaitBusFree:
		if (!(ctrl & (S_BSY|S_SEL)))
			update_state(State::SelectionAssertSEL, 10);
		break;

	case State::SelectionAssertSEL:
		scsi_bus->data_w(scsi_refid, m_temp);
		scsi_set_ctrl(0, S_SEL); //XXX: needed?
		scsi_set_ctrl(S_SEL | (m_send_atn_during_selection ? S_ATN : 0), S_ATN|S_SEL|S_BSY);
		scsi_bus->ctrl_wait(scsi_refid, S_BSY, S_BSY);
		update_state(State::SelectionWaitBSY, 0, m_tc / 8);
		break;

	case State::SelectionWaitBSY:
		if (timeout) {
			LOG("select timeout\n");
			scsi_disconnect_timeout();
			break;
		}
		m_timer->reset();
		if ((ctrl & (S_REQ|S_BSY|S_MSG|S_CTL|S_INP)) == S_BSY)
			update_state(State::Selection, 1);

		break;

	case State::Selection:
		LOG("selection success\n");
		scsi_set_ctrl(0, S_SEL);
		m_ssts |= SSTS_INIT_CONNECTED|SSTS_XFER_IN_PROGRESS;
		m_ssts &= ~(SSTS_TARG_CONNECTED|SSTS_SPC_BUSY);
		update_ssts();
		scsi_command_complete();
		break;

	case State::TransferWaitReq:
		if (!m_tc && !(m_scmd & SCMD_TERM_MODE)) {
			LOG("TransferWaitReq: tc == 0\n");
			scsi_bus->data_w(scsi_refid, 0);
			scsi_command_complete();
			break;
		}

		if (m_scsi_phase != (ctrl & S_PHASE_MASK)) {
			LOG("SCSI phase change during transfer\n");
			m_ints |= INTS_SERVICE_REQUIRED;
			m_ssts &= ~SSTS_SPC_BUSY;
			update_ints();
			break;
		}

		if (!(ctrl & S_REQ)) {
			scsi_bus->ctrl_wait(scsi_refid, S_REQ, S_REQ);
			break;
		}

		if (!m_dma_transfer || (m_scmd & SCMD_TERM_MODE)) {
			update_state((ctrl & S_INP) ? State::TransferRecvData : State::TransferSendData, 1);
		} else {
			update_state((ctrl & S_INP) ? State::TransferRecvDataDMAReq : State::TransferSendDataDMAReq, 1);
		}
		break;

	case State::TransferRecvData:
		if (!m_tc && (m_scmd & SCMD_TERM_MODE)) {
			update_state(State::TransferSendAck, 10);
			break;
		}

		if (!m_tc || m_fifo.full())
			break;

		LOG("pushing read data: %02X\n", data);
		m_fifo.enqueue(data);
		m_tc--;

		update_state(State::TransferSendAck, 10);
		break;

	case State::TransferRecvDataDMAReq:
		m_hdb = data;
		m_hdb_loaded = true;
		update_state(State::TransferRecvDataDMAResp, 10);
		m_tc--;
		m_dreq_handler(true);
		break;

	case State::TransferRecvDataDMAResp:
		if (m_hdb_loaded)
			break;
		update_state(State::TransferSendAck, 10);
		m_dreq_handler(false);
		break;

	case State::TransferSendData:
		if (m_tc && !m_fifo.empty()) {
			scsi_bus->data_w(scsi_refid, m_fifo.dequeue());
			m_tc--;
			update_state(State::TransferSendAck, 10);
			break;
		}

		if (!m_tc  && (m_scmd & SCMD_TERM_MODE)) {
			scsi_bus->data_w(scsi_refid, m_temp);
			update_state(State::TransferSendAck, 10);
			break;
		}
		break;

	case State::TransferSendDataDMAReq:
		m_hdb_loaded = false;
		update_state(State::TransferSendDataDMAResp, 10);
		m_dreq_handler(true);
		break;

	case State::TransferSendDataDMAResp:
		if (!m_hdb_loaded)
			break;
		m_hdb_loaded = false;
		m_dreq_handler(false);
		scsi_bus->data_w(scsi_refid, m_hdb);
		m_tc--;
		update_state(State::TransferSendAck, 10);
		break;

	case State::TransferSendAck:
		if (!(m_scmd & SCMD_TERM_MODE) && !(ctrl & S_INP))
				m_temp = data;

			scsi_set_ctrl(S_ACK, S_ACK);
			scsi_bus->ctrl_wait(scsi_refid, 0, S_REQ);

		update_state(State::TransferWaitDeassertREQ, 10);
		break;

	case State::TransferWaitDeassertREQ:
		if (!(ctrl & S_REQ))
			update_state(State::TransferDeassertACK, 10);
		break;

	case State::TransferDeassertACK:
		scsi_set_ctrl(0, S_ACK);
		scsi_bus->ctrl_wait(scsi_refid, S_REQ, S_REQ);
		update_state(State::TransferWaitReq, 10);
		break;

		}
}

void mb87030_device::device_start()
{
	m_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(mb87030_device::timeout), this));
	m_delay_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(mb87030_device::delay_timeout), this));
	m_irq_handler.resolve_safe();
	m_dreq_handler.resolve_safe();

	save_item(NAME(m_bdid));
	save_item(NAME(m_sctl));
	save_item(NAME(m_scmd));
	save_item(NAME(m_tmod));
	save_item(NAME(m_ints));
	save_item(NAME(m_sdgc));
	save_item(NAME(m_ssts));
	save_item(NAME(m_serr));
	save_item(NAME(m_pctl));
	save_item(NAME(m_mbc));
	save_item(NAME(m_dreg));
	save_item(NAME(m_temp));
	save_item(NAME(m_tch));
	save_item(NAME(m_tcm));
	save_item(NAME(m_tc));
	save_item(NAME(m_exbf));
	save_item(NAME(m_hdb));
	save_item(NAME(m_hdb_loaded));
	save_item(NAME(m_send_atn_during_selection));
//  save_item(NAME(m_fifo));
	save_item(NAME(m_scsi_phase));
	save_item(NAME(m_scsi_ctrl));
	save_item(NAME(m_dma_transfer));
//  save_item(NAME(m_state));
}

void mb87030_device::scsi_ctrl_changed()
{
	if (m_delay_timer->remaining() == attotime::never)
		step(false);
}

WRITE_LINE_MEMBER(mb87030_device::reset_w)
{
	LOG("%s: %s\n", __FUNCTION__, state);
	if (state)
		device_reset();
}

void mb87030_device::update_ssts()
{
	if (!m_tc)
		m_ssts |= SSTS_TC_ZERO;
	else
		m_ssts &= ~SSTS_TC_ZERO;

	if (m_fifo.empty())
		m_ssts |= SSTS_DREQ_EMPTY;
	else
		m_ssts &= ~SSTS_DREQ_EMPTY;

	if (m_fifo.full())
		m_ssts |= SSTS_DREQ_FULL;
	else
		m_ssts &= ~SSTS_DREQ_FULL;
}

void mb87030_device::update_ints()
{
	LOG("%s: %s\n", __FUNCTION__, (m_ints && (m_sctl & 1)) ? "true" : "false");
	m_irq_handler(m_ints && (m_sctl & 1));// || (m_ints & INTS_DISCONNECTED));
}

READ8_MEMBER(mb87030_device::bdid_r)
{
	LOG("%s %02X\n", __FUNCTION__, (1 << m_bdid));
	return 1 << m_bdid;
}

WRITE8_MEMBER(mb87030_device::bdid_w)
{
	LOG("%s: %02X\n", __FUNCTION__, data);
	m_bdid = data & 0x7;
}

READ8_MEMBER(mb87030_device::sctl_r)
{
	LOG("%s: %02X\n", __FUNCTION__, m_sctl);
	return m_sctl;
}

WRITE8_MEMBER(mb87030_device::sctl_w)
{
	LOG("%s: %02X\n", __FUNCTION__, data);
	m_sctl = data;
	update_ints();
}

READ8_MEMBER(mb87030_device::scmd_r)
{
	LOG("%s: %02X\n", __FUNCTION__, m_scmd);
	return m_scmd;
}

WRITE8_MEMBER(mb87030_device::scmd_w)
{
	LOG("%s: %02X\n", __FUNCTION__, data);
	m_scmd = data;

	switch (m_scmd & SCMD_CMD_MASK) {
	case SCMD_CMD_BUS_RELEASE:
		LOG("%s: Bus release\n", __FUNCTION__);
		break;

	case SCMD_CMD_SELECT:
		LOG("%s: Select\n", __FUNCTION__);
		if (m_sctl & SCTL_ARBITRATION_ENABLE)
			update_state(State::ArbitrationWaitBusFree, 10);
		else
			update_state(State::SelectionWaitBusFree, 10);
		step(false);
		break;

	case SCMD_CMD_RESET_ATN:
		LOG("%s: Reset ATN\n", __FUNCTION__);
//      if (m_state == State::Idle)
			m_send_atn_during_selection = false;
//      else
			scsi_set_ctrl(0, S_ATN);
		break;

	case SCMD_CMD_SET_ATN:
		LOG("%s: Set ATN\n", __FUNCTION__);
		if (m_state == State::Idle)
			m_send_atn_during_selection = true;
		else
			scsi_set_ctrl(S_ATN, S_ATN);
		break;

	case SCMD_CMD_TRANSFER:
		if (!(m_ssts & (SSTS_INIT_CONNECTED|SSTS_TARG_CONNECTED)))
			break;

		m_dma_transfer = !(data & 0x04);
		LOG("%s Transfer\n", m_dma_transfer ? "DMA" : "Program");
		m_ssts |= SSTS_SPC_BUSY|SSTS_XFER_IN_PROGRESS;
		update_state(State::TransferWaitReq, 5);
		break;

	case SCMD_CMD_TRANSFER_PAUSE:
		LOG("%s: Transfer Pause\n", __FUNCTION__);
		break;

	case SCMD_CMD_RESET_ACK_REQ:
		LOG("%s: Reset ACK/REQ\n", __FUNCTION__);
		if (m_ssts & SSTS_INIT_CONNECTED)
			scsi_set_ctrl(0, S_ACK);
		if (m_ssts & SSTS_TARG_CONNECTED)
			scsi_set_ctrl(0, S_REQ);
		break;

	case SCMD_CMD_SET_ACK_REQ:
		LOG("%s: Set ACK/REQ\n", __FUNCTION__);
		if (m_ssts & SSTS_INIT_CONNECTED) {
				if (scsi_bus->ctrl_r() & S_INP) {
					m_temp = scsi_bus->data_r();
				} else {
					scsi_bus->data_w(scsi_refid, m_temp);
				}
				LOG("set ACK\n");
				scsi_set_ctrl(S_ACK, S_ACK);
		} else {
			logerror("not connected\n");
		}
		if (m_ssts & SSTS_TARG_CONNECTED)
			scsi_set_ctrl(S_REQ, S_REQ);
		break;
	}
}

READ8_MEMBER(mb87030_device::tmod_r)
{
	LOG("%s: %02X\n", __FUNCTION__, m_tmod);
	return m_tmod;
}

WRITE8_MEMBER(mb87030_device::tmod_w)
{
	LOG("%s: %02X\n", __FUNCTION__, data);
	m_tmod = data;
}

READ8_MEMBER(mb87030_device::ints_r)
{
	LOG("%s: %02X\n", __FUNCTION__, m_ints);
	return m_ints;
}

WRITE8_MEMBER(mb87030_device::ints_w)
{
	LOG("%s: %02X\n", __FUNCTION__, data);
	m_ints &= ~(data);
	update_ints();
}

READ8_MEMBER(mb87030_device::psns_r)
{
	uint32_t ctrl = scsi_get_ctrl();
	uint8_t ret  = (!!(ctrl & S_REQ) << 7) |
			(!!(ctrl & S_ACK) << 6) |
			(!!(ctrl & S_ATN) << 5) |
			(!!(ctrl & S_SEL) << 4) |
			(!!(ctrl & S_BSY) << 3) |
			(!!(ctrl & S_MSG) << 2) |
			(!!(ctrl & S_CTL) << 1) |
			(!!(ctrl & S_INP));
	LOG("%s: %02X\n", __FUNCTION__, ret);
	return ret;
}

WRITE8_MEMBER(mb87030_device::sdgc_w)
{
	LOG("%s: %02X\n", __FUNCTION__, data);
	m_sdgc = data;
	scsi_ctrl_changed();
}

READ8_MEMBER(mb87030_device::ssts_r)
{
	LOG("%s: %02X\n", __FUNCTION__, m_ssts);
	update_ssts();
	return m_ssts;
}

READ8_MEMBER(mb87030_device::serr_r)
{
	LOG("%s: %02X\n", __FUNCTION__, m_serr);
	return m_serr;
}

READ8_MEMBER(mb87030_device::pctl_r)
{
	LOG("%s: %02X\n", __FUNCTION__, m_pctl);
	return m_pctl;
}

WRITE8_MEMBER(mb87030_device::pctl_w)
{
	LOG("%s: %02X\n", __FUNCTION__, data);
	m_pctl = data;
	m_scsi_phase = m_pctl & 7;
	if (m_pctl & PCTL_BUS_FREE_IE)
		step(false);
}

READ8_MEMBER(mb87030_device::mbc_r)
{
	LOG("%s: %02X\n", __FUNCTION__, m_mbc);
	return m_mbc;
}

READ8_MEMBER(mb87030_device::dreg_r)
{
	step(false);
	if (!m_fifo.empty())
			m_dreg = m_fifo.dequeue();
	LOG("%s: %02X\n", __FUNCTION__, m_dreg);
	return m_dreg;
}

WRITE8_MEMBER(mb87030_device::dreg_w)
{
	LOG("%s: %02X\n", __FUNCTION__, data);
	m_dreg = data;
	if (!m_fifo.full())
		m_fifo.enqueue(data);
	step(false);
}

READ8_MEMBER(mb87030_device::temp_r)
{
	step(false);
	LOG("%s: %02X\n", __FUNCTION__, m_temp);
	return m_temp;
}

WRITE8_MEMBER(mb87030_device::temp_w)
{
	LOG("%s: %02X\n", __FUNCTION__, data);
	m_temp = data;
	step(false);
}

READ8_MEMBER(mb87030_device::tch_r)
{
	uint8_t ret = (m_tc >> 16) & 0xff;
	LOG("%s: %02X\n", __FUNCTION__, ret);
	return ret;
}

READ8_MEMBER(mb87030_device::tcm_r)
{
	uint8_t ret = (m_tc >> 8) & 0xff;
	LOG("%s: %02X\n", __FUNCTION__, ret);
	return ret;
}

READ8_MEMBER(mb87030_device::tcl_r)
{
	uint8_t ret = m_tc & 0xff;
	LOG("%s: %02X\n", __FUNCTION__, ret);
	return ret;
}


WRITE8_MEMBER(mb87030_device::tch_w)
{
	LOG("%s: %02X\n", __FUNCTION__, data);
	m_tc &= 0x00ffff;
	m_tc |= data << 16;
	update_ssts();
}

WRITE8_MEMBER(mb87030_device::tcm_w)
{
	LOG("%s: %02X\n", __FUNCTION__, data);
	m_tc &= 0xff00ff;
	m_tc |= data << 8;
	update_ssts();
}

WRITE8_MEMBER(mb87030_device::tcl_w)
{
	LOG("%s: %02X\n", __FUNCTION__, data);
	m_tc &= 0xffff00;
	m_tc |= data;
	update_ssts();
}

READ8_MEMBER(mb87030_device::exbf_r)
{
	LOG("%s: %02X\n", __FUNCTION__, m_exbf);
	return m_exbf;
}

WRITE8_MEMBER(mb87030_device::exbf_w)
{
	LOG("%s: %02X\n", __FUNCTION__,  data);
	m_exbf = data;
}

void mb87030_device::dma_w(uint8_t data)
{
	if (machine().side_effects_disabled())
		return;
	LOG("dma_w: %02X\n", data);
	m_hdb = data;
	m_hdb_loaded = true;
	step(false);
}

uint8_t mb87030_device::dma_r(void)
{
	uint8_t val = m_hdb;

	if (machine().side_effects_disabled())
		return 0;
	LOG("dma_r: %02X\n", val);
	m_hdb_loaded = false;
	step(false);
	return val;
}
