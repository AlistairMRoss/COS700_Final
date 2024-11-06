import os
import subprocess
import requests
import csv
import json

def flash_certs():
  csv_data = [
        ["key", "type", "encoding", "value"],
        ["cos700","namespace", "", ""],
        ["deviceId", "data", "string", 'edc5ae89-fc3a-4f07-afc3-4ef7d9c943b3'], #input device ID
        ["endPoint","data","string", 'azf4s1s2y3qso-ats.iot.us-east-1.amazonaws.com'],
        ["cert", "file", "string", os.path.abspath("cert.pem")],
        ["private", "file", "string", os.path.abspath("private.key")],
        ["root", "data", "string", "false"]
  ]
  with open("output.csv", "w", newline='') as csvfile:
        csv_writer = csv.writer(csvfile)
        csv_writer.writerows(csv_data)
    
  idf_path = os.environ.get("IDF_PATH")
  if not idf_path:
    raise ValueError("ESP-IDF path not found. Make sure to set the IDF_PATH environment variable.")
  partool = f"{idf_path}/components/partition_table/parttool.py"
  gentool = f"{idf_path}/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py"

  gencommand = f"python {gentool} generate output.csv nvs.bin 0x3000"
  print(f"Generating NVS partition...")
  subprocess.run(gencommand, shell=True, check=True)

  writecommand = f"python {partool} write_partition --partition-name=nvs --input nvs.bin"
  print(f"Writing NVS partition...")
  subprocess.run(writecommand, shell=True, check=True)
  print("NVS partition flashed successfully.")

def upload_firmware():
    # Set up the ESP-IDF environment
    idf_path = os.environ.get("IDF_PATH")
    if not idf_path:
        raise ValueError("ESP-IDF path not found. Make sure to set the IDF_PATH environment variable.")
 
    # Flash the firmware
    boot_app = "./boot_app0.bin"
    partitions = "./partitions.bin"
    bootloader = "./bootloader.bin"
    app = "./firmware.bin"

    esptool = f"{idf_path}/components/esptool_py/esptool/esptool.py"
    flash_cmd = f"python {esptool} --chip esp32s3  --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0000 {bootloader} 0x8000 {partitions} 0xe000 {boot_app} 0x10000 {app}"
    print(f"Flashing firmware to ESP32-s3 on port...")
    subprocess.run(flash_cmd, shell=True, check=True)

    print("Firmware upload completed successfully.")

if __name__ == "__main__":
    # Replace COM3 with the appropriate port for your ESP32-C3
    upload_firmware()
    # flash_certs()
