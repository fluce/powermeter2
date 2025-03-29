async function readLoop(port, reader, setLogs, convert) {
  try {
    while (port.readable) {
      const { value, done } = await reader.read();
      if (done) {
        break;
      }
      const newLine = value;
      console.log(newLine);
      setLogs((prevLogs) => {
        let isReadingBuffer = prevLogs.isReadingBuffer;
        const addedToLogs = [];
        const addedToBuffer = [];
        let buffer = prevLogs.buffer;
        let bufferComplete = false;
        if (isReadingBuffer) {
          if (newLine.startsWith("Channel")) {
            bufferComplete = true;
            isReadingBuffer = false;
            addedToLogs.push(newLine);
          } else {
            addedToBuffer.push(newLine);
          }
        } else {
          if (newLine.startsWith("Buffer full")) {
            isReadingBuffer = true;
            buffer = [];
          } else {
            addedToLogs.push(newLine);
          }
        }
        let channelsValues = prevLogs.channelsValues;
        let channelTypes = prevLogs.channelTypes || {}; // Track channel types (voltage or current)

        if (addedToLogs.length > 0) {
          const lineData = Object.fromEntries(
            newLine.split("\t")
              .map(x => x.split("=", 2))
              .map(([k, v]) => [k, isNaN(v) ? v : parseFloat(v)])
          );
          if (lineData.Channel !== undefined) {
            channelsValues[lineData.Channel] = {
              channel: lineData.Channel,
              Vrms: lineData.Vrms,
              Irms: lineData.Irms,
              unit: lineData.Unit,
              time: lineData.Time,
              samplesCount: lineData.SamplesCount,
              mean: lineData.Mean
            };
            // Remember if the channel is voltage or current
            if (lineData.Vrms !== undefined) {
              channelTypes[lineData.Channel] = 'Voltage';
            } else if (lineData.Irms !== undefined) {
              channelTypes[lineData.Channel] = 'Current';
            }
          }
        }

        const res = {
          logs: [...prevLogs.logs, ...addedToLogs].slice(-101),
          buffer: bufferComplete ? [] : [...buffer, ...addedToBuffer],
          isReadingBuffer,
          channelsData: bufferComplete ? convert(buffer) : prevLogs.channelsData,
          channelsValues,
          channelTypes // Persist channel types
        };
        return res;
      });
    }
  } finally {
    reader.releaseLock();
  }
}

export default readLoop;
