"""Algebra checks for the conditional model in docs/hardware/EG-model-audit.md.

Run with Python 3; no third-party dependencies. These checks do not establish
transistor parameters, operating-region validity, or hardware fidelity.
"""

import math
import unittest


def solve(v, resistance, v_th, r_th, beta, high=0.0):
    """Evaluate the fixed-drop approximation with illustrative constants."""
    base_r = load_r = 47000.0
    capacitance = 2.2e-6
    be12 = be11 = 0.65
    bc11 = 0.55
    ce11 = be11 - bc11
    a = r_th / (beta + 1)
    denominator = 1 + a * (1 / load_r + 1 / resistance + 1 / base_r)
    p = (v_th - be12 - a * (
        9 / load_r - ce11 / resistance - (high - bc11) / base_r
    )) / denominator
    q = a / (resistance * denominator)
    sustain = p + q * v
    base11 = (high - sustain - bc11) / base_r
    emitter11 = (sustain - ce11 - v) / resistance
    collector11 = emitter11 - base11
    emitter12 = (sustain + 9) / load_r + collector11
    return dict(sustain=sustain, base11=base11, emitter11=emitter11,
                collector11=collector11, emitter12=emitter12, a=a,
                p=p, q=q, target=(p - ce11) / (1 - q),
                tau=resistance * capacitance / (1 - q))


class ConditionalDecayEquationTest(unittest.TestCase):
    def test_current_and_voltage_balance(self):
        for x in (0.0, 0.25, 0.5, 1.0):
            v_th = -90 * x / 22
            r_th = (10000 * x) * (22000 - 10000 * x) / 22000
            for beta in (120, 240):
                for resistance in (1800, 501800, 2001800):
                    for v in (-8.0, -4.0, -0.1):
                        with self.subTest(x=x, beta=beta, r=resistance, v=v):
                            s = solve(v, resistance, v_th, r_th, beta)
                            self.assertAlmostEqual(
                                s['emitter11'], s['base11'] + s['collector11'],
                                delta=1e-14)
                            self.assertAlmostEqual(
                                s['sustain'], v_th - 0.65 - s['a'] * s['emitter12'],
                                delta=1e-12)
                            derivative = s['emitter11'] / 2.2e-6
                            self.assertAlmostEqual(
                                derivative, (s['target'] - v) / s['tau'],
                                delta=1e-9)
                            self.assertGreaterEqual(s['q'], 0)
                            self.assertLess(s['q'], 1)
                            self.assertTrue(math.isfinite(s['tau']))

    def test_negative_collector_current_is_not_positive_load(self):
        s = solve(-4.0, 2001800, -90 / 22, 120000000 / 22000, 120)
        self.assertLess(s['collector11'], 0)

    def test_invalid_forward_active_tr12_case_is_detectable(self):
        s = solve(-0.1, 1800, -90 / 22, 120000000 / 22000, 120)
        self.assertLess(s['emitter12'], 0)
        # Algebra still has a solution; this is not a valid forward-active point.

    def test_zero_thevenin_resistance_removes_feedback(self):
        s = solve(-8.0, 1800, 0.0, 0.0, 120)
        self.assertEqual(s['q'], 0)
        self.assertAlmostEqual(s['sustain'], -0.65)
        self.assertAlmostEqual(s['tau'], 1800 * 2.2e-6)


if __name__ == '__main__':
    unittest.main()