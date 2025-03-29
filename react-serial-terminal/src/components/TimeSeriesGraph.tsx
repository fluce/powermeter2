import React from 'react';
import { RawChannelData } from '../types';

interface TimeSeriesGraphProps {
  svgRef: React.RefObject<SVGSVGElement|null>;
  logs: {
    channelsData: Array<Array<RawChannelData>>;
    channelTypes?: Record<number, 'Voltage' | 'Current'>;
  };
  visibleChannels: Record<number, boolean>;
  setVisibleChannels: React.Dispatch<React.SetStateAction<Record<number, boolean>>>;
  cursorData: Record<number, RawChannelData>;
}

function TimeSeriesGraph({
  svgRef,
  logs,
  visibleChannels,
  setVisibleChannels,
  cursorData,
}: TimeSeriesGraphProps) {
  const handleCheckboxChange = (index: number) => {
    setVisibleChannels((prev) => ({
      ...prev,
      [index]: !prev[index],
    }));
  };

  const voltageChannels = Object.entries(logs.channelTypes || {})
    .filter(([_, type]) => type === 'Voltage')
    .map(([index]) => parseInt(index));

  const currentChannels = Object.entries(logs.channelTypes || {})
    .filter(([_, type]) => type === 'Current')
    .map(([index]) => parseInt(index));

  return (
    <div>
      <svg ref={svgRef} width="800" height="400" style={{ backgroundColor: 'lightgray' }}></svg>
      <div style={{ display: 'flex', gap: '20px', marginTop: '10px' }}>
        <div>
          {voltageChannels.map((index) => (
            <div key={index}>
              <input
                type="checkbox"
                checked={visibleChannels[index] !== false}
                onChange={() => handleCheckboxChange(index)}
              />
              <label>
                Channel {index} (Voltage)
                {cursorData[index] ? `: ${cursorData[index].data}` : ''}
              </label>
            </div>
          ))}
        </div>
        <div>
          {currentChannels.slice(0, 3).map((index) => (
            <div key={index}>
              <input
                type="checkbox"
                checked={visibleChannels[index] !== false}
                onChange={() => handleCheckboxChange(index)}
              />
              <label>
                Channel {index} (Current)
                {cursorData[index] ? `: ${cursorData[index].data}` : ''}
              </label>
            </div>
          ))}
        </div>
        <div>
          {currentChannels.slice(3, 6).map((index) => (
            <div key={index}>
              <input
                type="checkbox"
                checked={visibleChannels[index] !== false}
                onChange={() => handleCheckboxChange(index)}
              />
              <label>
                Channel {index} (Current)
                {cursorData[index] ? `: ${cursorData[index].data}` : ''}
              </label>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}

export default TimeSeriesGraph;
