import React from 'react';

function TimeSeriesGraph({ svgRef, logs, visibleChannels, setVisibleChannels, cursorData }) {
  const handleCheckboxChange = (index) => {
    setVisibleChannels((prev) => ({
      ...prev,
      [index]: !prev[index]
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
        {/* Voltage Channels */}
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

        {/* Current Channels - Column 1 */}
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

        {/* Current Channels - Column 2 */}
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
