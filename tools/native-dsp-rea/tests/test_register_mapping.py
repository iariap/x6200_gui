#!/usr/bin/env python3
"""QA19 — tests offline de la reconstruccion GUI->X6200Control->registro.

Verifica SOLO la capa de mapeo/codificacion reconstruida del codigo fuente real
(src/control.c de X6200Control y src/radio.c, src/cfg/cfg.c, src/cfg/mode.c de la GUI).
NO valida el algoritmo DSP del firmware (FPGA/DSP no reconstruible offline): eso queda PENDING.
"""
import unittest

# --- enumeracion x6200_cmd_enum_t (include/.../low/control.h) ---
REG = dict(vfoa_agc=5, vfob_agc=11, sple_atue_trx=12, vi_vm=13, rxvol=14, rfg_txpwr=15,
           atu_network=17, nr_nb=23, dnf=24, cmplevel=25, agc=27, agctime=28,
           moni_fft=29, rxfilter=30, ks_km=33, qsk=34, rxeq=35, rxeqwfm=36, miceq=37,
           bias=41, rit=42, xit=43, filter_ssb=45, filter_ssb_2=46, filter_cw=47,
           filter_am=48, filter_nfm=49, filter_wfm=50)


def rx_filter_set(low, high):
    """x6200_control_rx_filter_set: val = ((uint32_t)low << 16) | high."""
    return (((low & 0xFFFF) << 16) | (high & 0xFFFF)) & 0xFFFFFFFF


def rx_filter_set_low(cur, low):
    """x6200_control_rx_filter_set_low: conserva word bajo, reemplaza word alto."""
    return (cur & 0x0000FFFF) | (((low & 0xFFFF) << 16) & 0xFFFFFFFF)


def rx_filter_set_high(cur, high):
    return (cur & 0xFFFF0000) | (high & 0xFFFF)


def nr_nb_value(nr_on, nr_level, nb_on, nb_level, nb_width):
    """x6200_nrthr_nbw_nbthr_nre_nbe: NR lvl [7:0], NB width [15:8],
    NB lvl [23:16], NR en bit24, NB en bit25."""
    return ((nr_level & 0xFF) | ((nb_width & 0xFF) << 8) | ((nb_level & 0xFF) << 16)
            | ((1 if nr_on else 0) << 24) | ((1 if nb_on else 0) << 25))


def dnf_value(center, width, mode):
    """x6200_dnfcnt_dnfwidth_dnfe: center [11:0], width [23:12], mode [25:24]."""
    return ((center & 0xFFF) | ((width & 0xFFF) << 12) | ((mode & 3) << 24))


def agc_value(knee_db, slope_db, hang_on):
    """x6200_agcknee_agcslope_agchang: knee [7:0] int8, slope [11:8], hang bit12."""
    return ((knee_db & 0xFF) | ((slope_db & 0xF) << 8) | ((1 if hang_on else 0) << 12))


def rx_eq_value(enabled, bands5):
    """x6200_rxeq: 5 campos de 5 bits (p1 [4:0] ... p5 [24:20]) + enable bit25."""
    assert len(bands5) == 5
    v = (1 if enabled else 0) << 25
    for i, p in enumerate(bands5):
        v |= (p & 0x1F) << (5 * i)
    return v


def s5(v):
    v &= 0x1F
    return v - 32 if v & 0x10 else v


class TestRegisterMapping(unittest.TestCase):
    def test_reg_indices(self):
        # Identidad observada en QA390: reg12 -> FPGA cmd 0x0C, reg24 -> FPGA cmd 0x18
        self.assertEqual(REG['sple_atue_trx'], 12)
        self.assertEqual(REG['dnf'], 24)
        self.assertEqual(REG['rxfilter'], 30)
        self.assertEqual(REG['rxeq'], 35)
        self.assertEqual([REG['filter_ssb'], REG['filter_ssb_2'], REG['filter_cw'],
                          REG['filter_am'], REG['filter_nfm'], REG['filter_wfm']],
                         [45, 46, 47, 48, 49, 50])

    def test_ssb_filter_pack(self):
        # LSB: low=50, high=2950 -> 0x0032_0B86
        self.assertEqual(rx_filter_set(50, 2950), 0x00320B86)
        # set_low/set_high preservan la otra mitad
        cur = rx_filter_set(50, 2950)
        self.assertEqual(rx_filter_set_low(cur, 100), 0x00640B86)
        self.assertEqual(rx_filter_set_high(cur, 2400), 0x00320960)

    def test_symmetric_modes(self):
        # radio.c: en CW/AM/NFM/WFM x6200_control_rx_filter_set(-high, high)
        high = 250   # CW default
        val = rx_filter_set(-high, high)
        self.assertEqual(val, 0xFF0600FA)   # -250 = 0xFF06; 250 = 0x00FA
        # AM default high=4500
        val = rx_filter_set(-4500, 4500)
        self.assertEqual(val, 0xEE6C1194)

    def test_nr_nb_fields(self):
        v = nr_nb_value(True, 5, True, 10, 20)
        self.assertEqual(v, 0x030A1405)
        self.assertEqual(v & 0xFF, 5)            # NR level
        self.assertEqual((v >> 8) & 0xFF, 20)    # NB width
        self.assertEqual((v >> 16) & 0xFF, 10)   # NB level
        self.assertTrue(v & (1 << 24))           # NR enable
        self.assertTrue(v & (1 << 25))           # NB enable

    def test_dnf_fields(self):
        v = dnf_value(1000, 50, 2)               # defaults GUI: center 1000, width 50; auto=2
        self.assertEqual(v, 0x02032000 | 1000)
        self.assertEqual(v & 0xFFF, 1000)
        self.assertEqual((v >> 12) & 0xFFF, 50)
        self.assertEqual((v >> 24) & 3, 2)

    def test_agc_fields(self):
        v = agc_value(-60, 6, False)             # defaults GUI: knee -60 dB, slope 6, no hang
        self.assertEqual(v & 0xFF, 0xC4)         # -60 como byte
        self.assertEqual((v >> 8) & 0xF, 6)
        self.assertEqual((v >> 12) & 1, 0)
        self.assertEqual(agc_value(-60, 6, True), 0x1000 | (-60 & 0xFF) | (6 << 8))

    def test_rx_eq_fields(self):
        v = rx_eq_value(True, [-16, -8, 0, 8, 15])
        self.assertTrue(v & (1 << 25))
        self.assertEqual([s5((v >> (5 * i)) & 0x1F) for i in range(5)], [-16, -8, 0, 8, 15])
        self.assertEqual(rx_eq_value(False, [0, 0, 0, 0, 0]), 0)
        # SDK enmascara con 0x1F: -1 se escribe 0x1F en el campo (two's complement 5 bits)
        v = rx_eq_value(False, [-1, 0, 0, 0, 0])
        self.assertEqual(v & 0x1F, 0x1F)

    def test_gui_mode_defaults_and_mirror(self):
        defaults = dict(lsb=(50, 2950), cw=(-250, 250), am=(0, 4500), nfm=(0, 3500), wfm=(0, 8000))
        self.assertEqual(defaults['lsb'], (50, 2950))
        # update_real_filters: LSB espeja dominio (from=-high, to=-low)
        low, high = defaults['lsb']
        self.assertEqual((-high, -low), (-2950, -50))
        # CW conserva low/high (incluye negativos)
        self.assertEqual(defaults['cw'], (-250, 250))
        # AM/NFM/WFM: low forzado a 0
        for m in ('am', 'nfm', 'wfm'):
            self.assertEqual(defaults[m][0], 0)

    def test_gui_limit_db_negative_rule(self):
        # mode.c: valores DB negativos se ignoran al cargar EXCEPTO en CW
        self.assertFalse(allowed_db_value(-250, mode='lsb'))
        self.assertTrue(allowed_db_value(-250, mode='cw'))
        self.assertTrue(allowed_db_value(50, mode='lsb'))


def allowed_db_value(val, mode):
    """mode.c::cfg_mode_params_load_item: (val<0) && pk != x6200_mode_cw -> ignora."""
    return not (val < 0 and mode != 'cw')


# --- defaults reales de la GUI (src/cfg/cfg.c) ---
def test_gui_defaults():
    d = dict(agc_hang=0, agc_knee=-60, agc_slope=6, dnf=0, dnf_center=1000, dnf_width=50,
             nb_level=10, nb_width=10, nr=0, nr_level=0, rx_bands=[0] * 5, rx_wfm_bands=[0] * 5,
             rx_en=0, rx_wfm_en=0)
    t = unittest.TestCase()
    t.assertEqual(agc_value(d['agc_knee'], d['agc_slope'], d['agc_hang']) & 0xFF, 0xC4)
    t.assertEqual(dnf_value(d['dnf_center'], d['dnf_width'], d['dnf']), 1000 | (50 << 12))
    t.assertEqual(nr_nb_value(d['nr'], d['nr_level'], 0, d['nb_level'], d['nb_width']),
                  10 << 16 | 10 << 8)
    t.assertEqual(rx_eq_value(d['rx_en'], d['rx_bands']), 0)


if __name__ == '__main__':
    unittest.main(verbosity=2)
