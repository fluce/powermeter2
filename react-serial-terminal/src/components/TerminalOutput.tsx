import React from 'react';

function* reverse<T>(arr: T[], maxItems: number): Generator<T> {
  for (let i = arr.length - 1; i >= 0 && maxItems > 0; i--, maxItems--) {
    yield arr[i];
  }
}

interface TerminalOutputProps {
  logs: string[];
}

function TerminalOutput({ logs }: TerminalOutputProps) {
  return (
    <div style={{ backgroundColor: 'black', color: 'white', padding: '10px', height: '400px', overflowY: 'scroll' }}>
      {[...reverse(logs, 100)].map((log, index) => (
        <div key={index}>{log}</div>
      ))}
    </div>
  );
}

export default TerminalOutput;
