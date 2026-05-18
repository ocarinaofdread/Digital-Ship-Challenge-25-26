import argparse
import os

IV_LEN = 4

def generate_sbox():
    s = list(range(256))
    seed = 0xA5

    for i in range(255, 0, -1):
        seed = (seed * 73 + 41) & 0xFF
        j = seed % (i + 1)
        s[i], s[j] = s[j], s[i] 
    return s

def invert_sbox(sbox):
    inv = [0] * 256
    for i, v in enumerate(sbox):
        inv[v] = i
    return inv


SBOX = generate_sbox()
INV_SBOX = invert_sbox(SBOX)

def encrypt(plaintext: str, key: str) -> str:
    iv = os.urandom(IV_LEN)
    iv_bytes = list(iv)

    key_len = len(key)
    output = []

    for i, ch in enumerate(plaintext):
        byte = ord(ch)
        key_byte = ord(key[i % key_len])
        byte ^= key_byte ^ iv_bytes[i % IV_LEN]
        byte = SBOX[byte]
        output.append(format(byte, '02x'))
    iv_hex = ''.join(format(b, '02x') for b in iv_bytes)
    return iv_hex + ''.join(output)


def decrypt(ciphertext: str, key: str) -> str:
    iv_bytes = [int(ciphertext[i:i+2], 16) for i in range(0, IV_LEN*2, 2)]
    data_bytes = [int(ciphertext[i:i+2], 16) for i in range(IV_LEN*2, len(ciphertext), 2)]

    key_len = len(key)
    output = []

    for i, byte in enumerate(data_bytes):
         key_byte = ord(key[i % key_len])
         iv_byte = iv_bytes[i % IV_LEN]
         byte = INV_SBOX[byte]
         byte ^= key_byte ^ iv_byte
         output.append(chr(byte))
    return ''.join(output)

def parse_app_args(arguments=None):
     parser = argparse.ArgumentParser()
     subparsers = parser.add_subparsers(dest='cmd', required=True)
    
     tx_parser = subparsers.add_parser('encrypt')
     tx_parser.add_argument('-m', '--msg', action='store', required=True, type=str)
     tx_parser.add_argument('-k', '--key', action='store', required=True, type=str)
    
     rx_parser = subparsers.add_parser('decrypt')
     rx_parser.add_argument('-m', '--msg', action='store', required=True, type=str)
     rx_parser.add_argument('-k', '--key', action='store', required=True, type=str)
    
     return parser.parse_args(arguments)

if __name__ == '__main__':
     parsed_args = parse_app_args()
     if parsed_args.cmd == 'decrypt':
         msg = parsed_args.msg
         key = parsed_args.key
         print(decrypt(msg, key))
     else:
         msg = parsed_args.msg
         key = parsed_args.key
         print(encrypt(msg, key))
