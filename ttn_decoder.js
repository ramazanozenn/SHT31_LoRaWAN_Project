function decodeUplink(input) {
  const data = input.bytes;
  
  // Örnek: SHT31 sıcaklık ve nem verisi
  const temperature = ((data[0] << 8) | data[1]) * 0.01;
  const humidity = ((data[2] << 8) | data[3]) * 0.01;
  
  return {
    data: {
      temperature: temperature,
      humidity: humidity.toFixed(2)
    }
  };
}
