import React from 'react';

function ChannelValuesDisplay({ channelsValues }) {
  return (
    <div style={{ display: 'flex', flexWrap: 'wrap', marginLeft: '10px' }}>
      {channelsValues.map((channelValue, index) => (
        <div key={index} style={{ border: '1px solid black', padding: '10px', margin: '5px', textAlign: 'center' }}>
          <div style={{ fontSize: '12px' }}>Channel {channelValue.channel}</div>
          <div style={{ fontSize: '24px' }}>
            {channelValue.Irms !== undefined ? `${channelValue.Irms} ${channelValue.unit}` : `${channelValue.Vrms} ${channelValue.unit}`}
          </div>
        </div>
      ))}
    </div>
  );
}

export default ChannelValuesDisplay;
