import React, { useState, useEffect, useRef } from 'react';
import LineBreakTransformer from './LineBreakTransformer';
import buildTimeSeriesGraph from './buildTimeSeriesGraph';
import SerialConnection from './components/SerialConnection';
import TerminalOutput from './components/TerminalOutput';
import TimeSeriesGraph from './components/TimeSeriesGraph';
import ChannelValuesDisplay from './components/ChannelValuesDisplay';
import readLoop from './readLoop';
import { LogState, RawChannelData } from './types';

function convert(buffer: string[]): Array<Array<RawChannelData>> {
  const channelsData: Array<Array<RawChannelData>> = [];
  let time = 0;
  for (let i = 0; i < buffer.length; i++) {
    const line = buffer[i];
    const fields = line.split(" ");
    const dt = fields[1];
    time += parseInt(dt);
    const channel = parseInt(fields[2]);
    const data = parseInt(fields[3]);
    const tab = channelsData[channel] ?? [];
    tab.push({ time, data });
    channelsData[channel] = tab;
  }
  return channelsData;
}

function* reverse<T>(arr: T[], maxItems: number): Generator<T> {
  for (let i = arr.length - 1; i >= 0 && maxItems > 0; i--, maxItems--) {
    yield arr[i];
  }
}

function App() {
  const [port, setPort] = useState<SerialPort | null>(null);
  const [reader, setReader] = useState<ReadableStreamDefaultReader<string> | null>(null);
  const [streamClosed, setStreamClosed] = useState<Promise<void> | null>(null);
  const [logs, setLogs] = useState<LogState>({
    logs: [],
    buffer: [],
    isReadingBuffer: false,
    channelsData: [],
    channelsValues: [],
  });
  const [error, setError] = useState<string | null>(null);
  const svgRef = useRef<SVGSVGElement>(null);
  const [visibleChannels, setVisibleChannels] = useState<Record<number, boolean>>({});
  const [cursorData, setCursorData] = useState<Record<number, RawChannelData>>({});

  useEffect(() => {
    if (port && reader) {
      readLoop(port, reader, setLogs, convert);
    }
  }, [port, reader]);

  useEffect(() => {
    buildTimeSeriesGraph(logs.channelsData, svgRef.current, visibleChannels, setCursorData);
  }, [logs.channelsData, visibleChannels]);

  return (
    <div>
      <SerialConnection
        port={port}
        setPort={setPort}
        reader={reader}
        setReader={setReader}
        streamClosed={streamClosed}
        setStreamClosed={setStreamClosed}
        setError={setError}
        error={error}
      />
      <TerminalOutput logs={logs.logs} />
      <div style={{ display: 'flex' }}>
        <TimeSeriesGraph
            svgRef={svgRef}
            logs={logs}
            visibleChannels={visibleChannels}
            setVisibleChannels={setVisibleChannels}
            cursorData={cursorData}
        />
        <ChannelValuesDisplay channelsValues={logs.channelsValues} />
      </div>
    </div>
  );
}

export default App;