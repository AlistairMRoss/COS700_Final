import hashlib

def calculate_firmware_hash(firmware_path):
    sha256_hash = hashlib.sha256()
    
    with open(firmware_path, 'rb') as firmware_file:
        while True:
            chunk = firmware_file.read(1024)
            if not chunk:
                break
            sha256_hash.update(chunk)
    
    binary_hash = sha256_hash.digest()
    
    hex_hash = ''.join([f'{byte:02x}' for byte in binary_hash])
    
    return hex_hash

def verify_firmware(firmware_path, expected_hash):
    calculated_hash = calculate_firmware_hash(firmware_path)
    hashes_match = calculated_hash.lower() == expected_hash.lower()
    
    print(f"Calculated hash: {calculated_hash}")
    print(f"Expected hash:   {expected_hash}")
    print("Hash verification SUCCESS" if hashes_match else "Hash verification FAILED")
    
    return hashes_match


if __name__ == "__main__":
    firmware_file = "firmware.bin"
    
    # Calculate hash only
    hash_value = calculate_firmware_hash(firmware_file)
    print(f"Firmware hash: {hash_value}")
