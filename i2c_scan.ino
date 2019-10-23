//***********************************************************************************************
//*
//*   I2C scan
//*
//***********************************************************************************************
void i2c_scan()
{
  byte error, address;
  int nDevices;

  SerialMon.println("Scanning...");

  nDevices = 0;
  for (address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      SerialMon.print("I2C device found at address 0x");
      if (address < 16)
        SerialMon.print("0");
      SerialMon.print(address, HEX);
      SerialMon.println("  !");

      nDevices++;
    }
    else if (error == 4)
    {
      SerialMon.print("Unknown error at address 0x");
      if (address < 16)
        SerialMon.print("0");
      SerialMon.println(address, HEX);
    }
  }
  if (nDevices == 0)
    SerialMon.println("No I2C devices found\n");
  else
    SerialMon.println("done\n");

  delay(5000);           // wait 5 seconds for next scan
}