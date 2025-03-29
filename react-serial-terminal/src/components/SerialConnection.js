import React from 'react';
import LineBreakTransformer from '../LineBreakTransformer';

function SerialConnection({ port, setPort, reader, setReader, streamClosed, setStreamClosed, setError, error }) {
  const connectSerial = async () => {
    try {
      const port = await navigator.serial.requestPort();
      await port.open({ baudRate: 9600 });
      const decoder = new TextDecoderStream();
      const streamClosed = port.readable.pipeTo(decoder.writable);
      const reader = decoder.readable
        .pipeThrough(new TransformStream(new LineBreakTransformer()))
        .getReader();
      setReader(reader);
      setStreamClosed(streamClosed);
      setPort(port);
      setError(null);
    } catch (err) {
      setError(`Failed to open serial port: ${err.message}`);
    }
  };

  const disconnectSerial = async () => {
    if (reader) {
      reader.cancel();
      await streamClosed.catch(() => {});
    }
    if (port) {
      await port.close();
    }
    setPort(null);
    setReader(null);
  };

  return (
    <div>
      <button onClick={connectSerial} disabled={port !== null}>Connect to Serial Port</button>
      <button onClick={disconnectSerial} disabled={!port}>Disconnect</button>
      {error && <div style={{ color: 'red' }}>{error}</div>}
    </div>
  );
}

export default SerialConnection;
