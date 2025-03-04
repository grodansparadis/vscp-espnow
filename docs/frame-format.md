```
| 0x55 | 0xAA | ttl | seq (1 byte) | Packet type & encryption settings (1 byte) || AES-128 CBC encrypted data (event) || IV (16-byte) |
```

| Offset | Real offset | Description | 
| ------ | ----------- | ----------- |
|  0     |	5      | VSCP Level II Head MSB |
|  1     |	6      | VSCP Level II Head LSB |
|  2     |	7      | Timestamp microseconds MSB |
|  3     |	8      | Timestamp microseconds |
|  4     |	9      | Timestamp microseconds |
|  5     |	10     | Timestamp microseconds LSB |
|  6     |	11     | CLASS MSB |
|  7    |	12     | CLASS LSB |
|  8     |	13     | TYPE MSB |
|  9     |	14     | TYPE LSB |
|  10    |      15     | len data |
|  11-n  |	16     | data ... limited to max 217 bytes |
|  len-2 |  16 + len   | CRC MSB (Calculated on HEAD + CLASS + TYPE + ADDRESS + SIZE + DATA…) |
|  len-1 |  17 + len   | CRC LSB |
  
  ```
  start
  ttl
  seq
  enc + type
  VSCP frame
  CRC
  IV
  ```
