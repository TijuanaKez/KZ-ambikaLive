#!/usr/bin/python2.5
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# -----------------------------------------------------------------------------

"""Tool for producing a clear text dump of Ambika program(s)."""

import optparse
import struct
import sys

OSCILLATOR_ALGORITHM = [
  'none', #'WAVEFORM_NONE',
  'saw', #'WAVEFORM_SAW',
  'square', #'WAVEFORM_SQUARE',
  'triangle', #'WAVEFORM_TRIANGLE',
  'sine', #'WAVEFORM_SINE',
  'zsaw', #'WAVEFORM_CZ_SAW',
  'lpzsaw', #'WAVEFORM_CZ_SAW_LP',
  'pkzsaw', #'WAVEFORM_CZ_SAW_PK',
  'bpzsaw', #'WAVEFORM_CZ_SAW_BP',
  'hpzsaw', #'WAVEFORM_CZ_SAW_HP',
  'lpzpulse', #'WAVEFORM_CZ_PLS_LP',
  'pkzpulse', #'WAVEFORM_CZ_PLS_PK',
  'bpzpulse', #'WAVEFORM_CZ_PLS_BP',
  'hpzpulse', #'WAVEFORM_CZ_PLS_HP',
  'ztriangle', #'WAVEFORM_CZ_TRI_LP',
  'pad', #""'WAVEFORM_QUAD_SAW_PAD',
  'fm', #'WAVEFORM_FM',
  '8bits', #'WAVEFORM_8BITLAND',
  'pwm', #'WAVEFORM_DIRTY_PWM',
  'noise', #'WAVEFORM_FILTERED_NOISE',
  'vowel', #'WAVEFORM_VOWEL',
  'male', #'WAVEFORM_WAVETABLE_MALE',
  'female', #'WAVEFORM_WAVETABLE_FEMALE',
  'choir', #'WAVEFORM_WAVETABLE_CHOIR',
  'tampura', #'WAVEFORM_WAVETABLE_TAMPURA',
  'bowed', #'WAVEFORM_WAVETABLE_BOWED',
  'cello', #'WAVEFORM_WAVETABLE_CELLO',
  'vibes', #'WAVEFORM_WAVETABLE_VIBES',
  'slap', #'WAVEFORM_WAVETABLE_SLAP',
  'epiano', #'WAVEFORM_WAVETABLE_EPIANO',
  'organ', #'WAVEFORM_WAVETABLE_ORGAN',
  'waves', #'WAVEFORM_WAVETABLE_WAVES',
  'digital', #'WAVEFORM_WAVETABLE_DIGITAL',
  'drone 1', #'WAVEFORM_WAVETABLE_DRONE1',  
  'drone 2', #'WAVEFORM_WAVETABLE_DRONE2',
  'metallic', #'WAVEFORM_WAVETABLE_METALLIC',
  'bell', #'WAVEFORM_WAVETABLE_BELLISH',
  'wavquence', #'WAVEFORM_WAVETABLE_WAVEQUENCE',
]

SUB_OSCILLATOR_ALG = [
  'squ1', #'WAVEFORM_SUB_OSC_SQUARE_1',
  'tri1', #'WAVEFORM_SUB_OSC_TRIANGLE_1',
  'pul1', #'WAVEFORM_SUB_OSC_PULSE_1',
  'squ2', #'WAVEFORM_SUB_OSC_SQUARE_2',
  'tri2', #'WAVEFORM_SUB_OSC_TRIANGLE_2',
  'pul2', #'WAVEFORM_SUB_OSC_PULSE_2',
  'click', #'WAVEFORM_SUB_OSC_CLICK',
  'glitch', #'WAVEFORM_SUB_OSC_GLITCH',
  'blow', #'WAVEFORM_SUB_OSC_BLOW',
  'metal', #'WAVEFORM_SUB_OSC_METALLIC',
  'pop', #'WAVEFORM_SUB_OSC_POP',
]

FILTER_MODE = [
  'lp', #'FILTER_MODE_LP',
  'bp', #'FILTER_MODE_BP',
  'hp', #'FILTER_MODE_HP',
  'notch', #'FILTER_MODE_NOTCH',
]

MODULATION_SOURCE = [
  'env1', #'MOD_SRC_ENV_1',
  'env2', #'MOD_SRC_ENV_2',
  'env3', #'MOD_SRC_ENV_3',
  'lfo1', #'MOD_SRC_LFO_1',
  'lfo2', #'MOD_SRC_LFO_2',
  'lfo3', #'MOD_SRC_LFO_3',
  'lfo4', #'MOD_SRC_LFO_4',
  'mod1', #'MOD_SRC_OP_1',
  'mod2', #'MOD_SRC_OP_2',
  'mod3', #'MOD_SRC_OP_3',
  'mod4', #'MOD_SRC_OP_4',
  'seq1', #'MOD_SRC_SEQ_1',
  'seq2', #'MOD_SRC_SEQ_2',
  'arp', #'MOD_SRC_ARP_STEP',
  'velo', #'MOD_SRC_VELOCITY',
  'aftr', #'MOD_SRC_AFTERTOUCH',
  'bend', #'MOD_SRC_PITCH_BEND',
  'mwhl', #'MOD_SRC_WHEEL',
  'whl2', #'MOD_SRC_WHEEL_2',
  'pdal', #'MOD_SRC_EXPRESSION',
  'note', #'MOD_SRC_NOTE',
  'gate', #'MOD_SRC_GATE',
  'nois', #MOD_SRC_NOISE',
  'rand', #'MOD_SRC_RANDOM',
  '=256', #'MOD_SRC_CONSTANT_256',
  '=128', #'MOD_SRC_CONSTANT_128',
  '=64', #'MOD_SRC_CONSTANT_64',
  '=32', #'MOD_SRC_CONSTANT_32',
  '=16', #'MOD_SRC_CONSTANT_16',
  '=8', #'MOD_SRC_CONSTANT_8',
  '=4', #'MOD_SRC_CONSTANT_4',
]

MODULATION_DESTINATION = [
  'prm1', #'MOD_DST_PARAMETER_1',
  'prm2', #'MOD_DST_PARAMETER_2',
  'osc1', #'MOD_DST_OSC_1',
  'osc2', #'MOD_DST_OSC_2',
  '1and2', #'MOD_DST_OSC_1_2_COARSE',
  'vibr', #'MOD_DST_OSC_1_2_FINE',
  'mix', #'MOD_DST_MIX_BALANCE',
  'xmod', #'MOD_DST_MIX_PARAM',
  'nois', #'MOD_DST_MIX_NOISE',
  'sub', #'MOD_DST_MIX_SUB_OSC',
  'fuzz', #'MOD_DST_MIX_FUZZ',
  'crsh', #'MOD_DST_MIX_CRUSH',
  'freq', #'MOD_DST_FILTER_CUTOFF',
  'reso', #MOD_DST_FILTER_RESONANCE',
  'attk', #'MOD_DST_ATTACK',
  'deca', #'MOD_DST_DECAY',
  'rele', #'MOD_DST_RELEASE',
  'lfo4', #'MOD_DST_LFO_4',
  'vca', #'MOD_DST_VCA',
]

OPERATOR = [
  'off', #'OP_SUM',
  'sync', #'OP_SYNC',
  'ring', #'OP_RING_MOD',
  'xor', #'OP_XOR',
  'fold', #'OP_FOLD',
  'bits', #'OP_BITS',
]

LFO_WAVE = [
  'tri', #'LFO_WAVEFORM_TRIANGLE',
  'sqr', #'LFO_WAVEFORM_SQUARE',
  's&h', #'LFO_WAVEFORM_S_H',
  'ramp', #'LFO_WAVEFORM_RAMP',
  'sine', #LFO_WAVEFORM_WAVE_1',
  'hrm2', #'LFO_WAVEFORM_WAVE_2',
  'hrm3', #'LFO_WAVEFORM_WAVE_3',
  'hrm5', #'LFO_WAVEFORM_WAVE_4',
  'grg1', #'LFO_WAVEFORM_WAVE_5',
  'grg2', #'LFO_WAVEFORM_WAVE_6',
  'bat1', #'LFO_WAVEFORM_WAVE_7',
  'bat2', #'LFO_WAVEFORM_WAVE_8',
  'spk1', #'LFO_WAVEFORM_WAVE_9',
  'spk2', #'LFO_WAVEFORM_WAVE_10',
  'lsaw', #LFO_WAVEFORM_WAVE_11',
  'lsqr', #LFO_WAVEFORM_WAVE_12',
  'rsaw', #'LFO_WAVEFORM_WAVE_13',
  'rsqr', #LFO_WAVEFORM_WAVE_14',
  'stp1', #'LFO_WAVEFORM_WAVE_15',
  'stp2', #'LFO_WAVEFORM_WAVE_16',
]

LFO_SYNC_MODE = [
  'free', #'LFO_SYNC_MODE_FREE',
  'env-lfo', #'LFO_SYNC_MODE_SLAVE',
  'lfo-env', #'LFO_SYNC_MODE_MASTER',
]

def signed_byte(b):
  if (b > 127):
    return -(256-b)
  else:
    return b

class AmbikaPatch(object):
  
  def __init__(self, bytes):
    self.oscillator_settings = []
    self.filter_settings = []
    self.envelope_lfo_settings = []
    self.modulations = []
    self.modifiers = []
    for i in xrange(2):
      self.oscillator_settings.append(
        {'shape': OSCILLATOR_ALGORITHM[bytes[i * 4 + 0]],
         'parameter': bytes[i * 4 + 1],
         'range': signed_byte(bytes[i * 4 + 2]),
         'detune': signed_byte(bytes[i * 4 + 3])})
    self.mix_balance = bytes[8]
    self.mix_op = OPERATOR[bytes[9]]
    self.mix_parameter = bytes[10]
    self.mix_sub_osc_shape = SUB_OSCILLATOR_ALG[bytes[11]]
    self.mix_sub_osc = bytes[12]
    self.mix_noise = bytes[13]
    self.mix_fuzz = bytes[14]
    self.mix_crush = bytes[15]
    for i in xrange(2):
      self.filter_settings.append(
        {'cutoff': bytes[i * 3 + 16],
         'resonance': bytes[i * 3 + 17],
         'mode': FILTER_MODE[bytes[i * 3 + 18]]})
    self.filter_env = bytes[22]
    self.filter_lfo = bytes[23]
    for i in xrange(3):
      self.envelope_lfo_settings.append(
        {'attack': bytes[i * 8 + 24],
         'decay': bytes[i * 8 + 25],
         'sustain': bytes[i * 8 + 26],
         'release': bytes[i * 8 + 27],
         'shape': LFO_WAVE[bytes[i * 8 + 28]],
         'rate': bytes[i * 8 + 29],
         'padding': bytes[i * 8 + 30],
         'retrigger_mode': LFO_SYNC_MODE[bytes[i * 8 + 31]]})
    self.voice_lfo_shape = LFO_WAVE[bytes[48]]
    self.voice_lfo_rate = bytes[49]
    for i in xrange(14):
      self.modulations.append(
        {'source': MODULATION_SOURCE[bytes[i * 3 + 50]],
         'destination': MODULATION_DESTINATION[bytes[i * 3 + 51]],
         'amount': signed_byte(bytes[i * 3 + 52])})
    for i in xrange(4):
      self.modifiers.append(
        {'operands': bytes[i * 2 + 92],
         'op': bytes[i * 2 + 93]})

def main(options, args):
  for f in args:
    data = file(f, 'rb').read()
    databytes = bytearray(data)
    patch = AmbikaPatch(databytes[48:160])
    sys.stdout.write(f + '\t')
    sys.stdout.write(data[20:35] + '\t')
    sys.stdout.write(patch.oscillator_settings[0]['shape'] + '\t')
    sys.stdout.write("%d" % patch.oscillator_settings[0]['parameter'] + '\t')
    sys.stdout.write("%d" % patch.oscillator_settings[0]['range'] + '\t')
    sys.stdout.write("%d" % patch.oscillator_settings[0]['detune'] + '\t')
    sys.stdout.write(patch.oscillator_settings[1]['shape'] + '\t')
    sys.stdout.write("%d" % patch.oscillator_settings[1]['parameter'] + '\t')
    sys.stdout.write("%d" % patch.oscillator_settings[1]['range'] + '\t')
    sys.stdout.write("%d" % patch.oscillator_settings[1]['detune'] + '\t')
    sys.stdout.write("%d" % patch.mix_balance + '\t')
    sys.stdout.write(patch.mix_op + '\t')
    sys.stdout.write("%d" % patch.mix_parameter + '\t')
    sys.stdout.write(patch.mix_sub_osc_shape + '\t')
    sys.stdout.write("%d" % patch.mix_sub_osc + '\t')
    sys.stdout.write("%d" % patch.mix_noise + '\t')
    sys.stdout.write("%d" % patch.mix_fuzz + '\t')
    sys.stdout.write("%d" % patch.mix_crush + '\t')
    for i in xrange(2):
      sys.stdout.write("%d" % patch.filter_settings[i]['cutoff'] + '\t')
      sys.stdout.write("%d" % patch.filter_settings[i]['resonance'] + '\t')
      sys.stdout.write(patch.filter_settings[i]['mode'] + '\t')
    sys.stdout.write("%d" % patch.filter_env + '\t')
    sys.stdout.write("%d" % patch.filter_lfo + '\t')
    for i in xrange(3):
      sys.stdout.write("%d" % patch.envelope_lfo_settings[i]['attack'] + '\t')
      sys.stdout.write("%d" % patch.envelope_lfo_settings[i]['decay'] + '\t')
      sys.stdout.write("%d" % patch.envelope_lfo_settings[i]['sustain'] + '\t')
      sys.stdout.write("%d" % patch.envelope_lfo_settings[i]['release'] + '\t')
      sys.stdout.write(patch.envelope_lfo_settings[i]['shape'] + '\t')
      sys.stdout.write("%d" % patch.envelope_lfo_settings[i]['rate'] + '\t')
      sys.stdout.write("%d" % patch.envelope_lfo_settings[i]['padding'] + '\t')
      sys.stdout.write(patch.envelope_lfo_settings[i]['retrigger_mode'] + '\t')
    sys.stdout.write(patch.voice_lfo_shape + '\t')
    sys.stdout.write("%d" % patch.voice_lfo_rate + '\t')
    for i in xrange(14):
        sys.stdout.write(patch.modulations[i]['source'] + '\t')
        sys.stdout.write(patch.modulations[i]['destination'] + '\t')
        sys.stdout.write("%d" % patch.modulations[i]['amount'] + '\t')
    for i in xrange(4):
      sys.stdout.write("%d" % patch.modifiers[i]['operands'] + '\t')
      sys.stdout.write("%d" % patch.modifiers[i]['op'] + '\t')
    sys.stdout.write('\n')

if __name__ == '__main__':
  parser = optparse.OptionParser()
  options, args = parser.parse_args()
  main(options, args)
