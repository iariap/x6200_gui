#!/usr/bin/env python3
"""Tests del verificador offline (deterministas, sin hardware)."""
import math, os, sys, unittest
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import analyze_audio_ab as m

class TestConsistency(unittest.TestCase):
    def test_constants_source(self):
        checks, vals = m.consistency()
        self.assertTrue(checks['capture_44100'])
        self.assertTrue(checks['enh_frame_441'])
        self.assertTrue(checks['scratch_4410'])
        self.assertTrue(checks['capture_only'])
        self.assertTrue(checks['router_not_in_base'])
        self.assertAlmostEqual(vals['block_ms'], 100.0, places=2)
        self.assertAlmostEqual(vals['ring_ms'], 400.0, places=2)
        self.assertAlmostEqual(vals['nr_latency_ms'], 20.0, places=2)
        self.assertAlmostEqual(vals['router_audio_s'], 1.8, places=3)

    def test_biquad_bypass_identity(self):
        x = [math.sin(2*math.pi*3*i/100) for i in range(100)]
        self.assertEqual(m.run_biquad(x, [1.0, 0, 0, 0, 0]), x)

    def test_bandpass_attenuates_out_of_band(self):
        out = m.measure_model()
        self.assertLess(out['bp_outband_atten_db'], -10.0)   # atenúa fuera de banda
        self.assertGreater(out['bp_inband_change_db'], -15.0)  # no destruye la señal útil

    def test_notch_attenuates_tone(self):
        out = m.measure_model()
        self.assertLess(out['notch_700_atten_db'], -6.0)

    def test_agc_no_clipping_in_model(self):
        out = m.measure_model()
        self.assertEqual(out['clip_before'], 0)
        self.assertEqual(out['clip_after'], 0)

if __name__ == '__main__':
    unittest.main(verbosity=2)
