import { ChannelValues, LogState, RawChannelData } from './types';

interface RawChannelValues {
    Channel: number;
    Vrms?: number;
    Irms?: number;
    Papp?: number;
    Preal?: number;
    Eact?: number;
    Ereact?: number;
    Unit?: string;
    Time?: number;
    SamplesCount?: number;
    Mean?: number;
}

async function readLoop(
  port: SerialPort,
  reader: ReadableStreamDefaultReader<string>,
  setLogs: React.Dispatch<React.SetStateAction<LogState>>,
  convert: (buffer: string[]) => Array<Array<RawChannelData>>
) {
  try {
    while (port.readable) {
      const { value, done } = await reader.read();
      if (done) {
        break;
      }
      const newLine = value!;
      console.log(newLine);
      setLogs((prevLogs:LogState) => {
        let isReadingBuffer = prevLogs.isReadingBuffer;
        const addedToLogs: string[] = [];
        const addedToBuffer: string[] = [];
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
          const lineData: RawChannelValues = Object.fromEntries(
            newLine.split("\t")
              .map(x => x.split("=", 2))
              .map(([k, v]) => [k, isNaN(v as any) ? v : parseFloat(v)])
          ) as unknown as RawChannelValues;
          if (lineData.Channel !== undefined) {
            channelsValues[lineData.Channel] = {
              channel: lineData.Channel,
              Vrms: lineData.Vrms,
              Irms: lineData.Irms,
              Papp: lineData.Papp,
              Preal: lineData.Preal,
              Eact: lineData.Eact,
              Ereact: lineData.Ereact,
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

        const res: LogState = {
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
