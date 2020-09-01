// license:BSD-3-Clause
// copyright-holders:AJR
/**********************************************************************

    Sitronix ST2205U 8-Bit Integrated Microcontroller

**********************************************************************/

#ifndef MAME_CPU_M6502_ST2205U_H
#define MAME_CPU_M6502_ST2205U_H

#pragma once

#include "st2xxx.h"

class st2205u_device : public st2xxx_device
{
public:
	enum {
		ST_BTC = ST_BDIV + 1,
		ST_T0C,
		ST_T1C,
		ST_T2C,
		ST_T3C,
		ST_T4C,
		ST_TIEN,
		ST_FIFOS0,
		ST_FIFOS1,
		ST_FIFOS2,
		ST_FIFOS3,
		ST_PSGC,
		ST_PSGM,
		ST_VOL0,
		ST_VOL1,
		ST_VOL2,
		ST_VOL3,
		ST_VOLM0,
		ST_VOLM1,
		ST_LBUF,
		ST_BRR,
		ST_DMS0,
		ST_DMS1,
		ST_DMD0,
		ST_DMD1,
		ST_DBKS0,
		ST_DBKS1,
		ST_DBKD0,
		ST_DBKD1,
		ST_DCNT0,
		ST_DCNT1,
		ST_DCTR,
		ST_DMOD0,
		ST_DMOD1,
		ST_USBCON,
		ST_USBIEN,
		ST_RCTR,
		ST_LVCTR
	};

	st2205u_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

	virtual u16 st2xxx_ireq_mask() const override { return 0xdfff; }
	virtual const char *st2xxx_irq_name(int i) const override;
	virtual u8 st2xxx_pmcr_mask() const override { return 0xff; }
	virtual unsigned st2xxx_bt_divider(int n) const override;
	virtual u8 st2xxx_prs_mask() const override { return 0xc0; }
	virtual void st2xxx_tclk_start() override;
	virtual void st2xxx_tclk_stop() override;
	virtual u8 st2xxx_sys_mask() const override { return 0xfe; }
	virtual u8 st2xxx_misc_mask() const override { return 0x0f; }
	virtual bool st2xxx_wdten_on_reset() const override { return true; }
	virtual bool st2xxx_has_dma() const override { return true; }
	virtual u8 st2xxx_lpan_mask() const override { return 0x0f; }
	virtual u8 st2xxx_lctr_mask() const override { return 0xef; }
	virtual u8 st2xxx_lckr_mask() const override { return 0x3f; }
	virtual u8 st2xxx_lpwm_mask() const override { return 0xff; }
	virtual unsigned st2xxx_lfr_clocks() const override;
	virtual u8 st2xxx_uctr_mask() const override { return 0x3f; }
	virtual u8 st2xxx_bctr_mask() const override { return 0xb7; }

private:
	class mi_st2205u : public mi_st2xxx {
	public:
		virtual u8 read(u16 adr) override;
		virtual u8 read_sync(u16 adr) override;
		virtual u8 read_arg(u16 adr) override;
		virtual u8 read_vector(u16 adr) override;
		virtual void write(u16 adr, u8 val) override;

		u8 pread(u16 adr);
		u8 preadc(u16 adr);
		void pwrite(u16 adr, u8 val);
		u8 dread(u16 adr);
		u8 dreadc(u16 adr);
		void dwrite(u16 adr, u8 val);
		u8 bread(u16 adr);
		u8 breadc(u16 adr);
		void bwrite(u16 adr, u8 val);

		u16 brr;

		std::unique_ptr<u8[]> ram;
	};

	template<int N> TIMER_CALLBACK_MEMBER(bt_interrupt);

	u8 brrl_r();
	void brrl_w(u8 data);
	u8 brrh_r();
	void brrh_w(u8 data);
	u8 btc_r();
	void btc_w(u8 data);
	u8 tc_12bit_r(offs_t offset);
	void tc_12bit_w(offs_t offset, u8 data);
	u8 t4c_r();
	void t4c_w(u8 data);
	u8 tien_r();
	void tien_w(u8 data);
	u8 psg_r(offs_t offset);
	void psg_w(offs_t offset, u8 data);
	u8 psgc_r();
	void psgc_w(u8 data);
	u8 psgm_r();
	void psgm_w(u8 data);
	u8 vol_r(offs_t offset);
	void vol_w(offs_t offset, u8 data);
	u8 volm_r(offs_t offset);
	void volm_w(offs_t offset, u8 data);
	u8 lbuf_r();
	void lbuf_w(u8 data);
	void lpal_w(u8 data);
	u8 usbcon_r();
	void usbcon_w(u8 data);
	u8 usbien_r();
	void usbien_w(u8 data);
	u8 dptrl_r();
	void dptrl_w(u8 data);
	u8 dptrh_r();
	void dptrh_w(u8 data);
	u8 dbkrl_r();
	void dbkrl_w(u8 data);
	u8 dbkrh_r();
	void dbkrh_w(u8 data);
	u8 dcntl_r();
	void dcntl_w(u8 data);
	u8 dcnth_r();
	void dcnth_w(u8 data);
	u8 dctr_r();
	void dctr_w(u8 data);
	u8 dmod_r();
	void dmod_w(u8 data);
	u8 rctr_r();
	void rctr_w(u8 data);
	u8 lvctr_r();
	void lvctr_w(u8 data);

	u8 ram_r(offs_t offset);
	void ram_w(offs_t offset, u8 data);
	u8 pmem_r(offs_t offset);
	void pmem_w(offs_t offset, u8 data);
	u8 dmem_r(offs_t offset);
	void dmem_w(offs_t offset, u8 data);
	u8 bmem_r(offs_t offset);
	void bmem_w(offs_t offset, u8 data);

	void int_map(address_map &map);

	u8 m_btc;
	u16 m_tc_12bit[4];
	u8 m_t4c;
	u8 m_tien;
	u16 m_dac_fifo[4][16];
	u8 m_fifo_filled[4];
	u8 m_psgc;
	u8 m_psgm;
	u8 m_psg_vol[4];
	u8 m_psg_volm[2];
	u8 m_lbuf;
	u8 m_lpal_index;
	u8 m_gray_levels[16];
	u8 m_usbcon;
	u8 m_usbien;
	u16 m_dptr[4];
	u16 m_dbkr[4];
	u16 m_dcnt[2];
	u8 m_dctr;
	u8 m_dmod[2];
	u8 m_rctr;
	u8 m_lvctr;
};

DECLARE_DEVICE_TYPE(ST2205U, st2205u_device)

#endif // MAME_MACHINE_M6502_ST2205_H
