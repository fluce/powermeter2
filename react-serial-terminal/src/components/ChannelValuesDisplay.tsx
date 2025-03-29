import React from 'react';
import { ChannelValues } from '../types';

interface ChannelValuesDisplayProps {
  channelsValues: Array<ChannelValues>;
}

function ChannelValuesDisplay({ channelsValues }: ChannelValuesDisplayProps) {
  return (
    <div style={{ display: 'flex', flexWrap: 'wrap', marginLeft: '10px' }}>
      {channelsValues.map((channelValue, index) => (
        <div key={index} style={{ border: '1px solid black', padding: '10px', margin: '5px', textAlign: 'center' }}>
          <div style={{ fontSize: '12px' }}>Channel {channelValue.channel}</div>
          <div style={{ fontSize: '24px' }}>
            {channelValue.Irms !== undefined
              ? `${channelValue.Irms.toFixed(2)} ${channelValue.unit}`
              : `${channelValue.Vrms?.toFixed(2)} ${channelValue.unit}`}
          </div>
          { (channelValue.Irms !== undefined) && 
          <>
            <div style={{ fontSize: '24px' }}>
                {channelValue.Preal !== undefined
                ? `${channelValue.Preal.toFixed(1)} W`
                : ''}
            </div>
            <div style={{ fontSize: '24px' }}>
                {channelValue.Papp !== undefined
                ? `${channelValue.Papp.toFixed(1)} VA`
                : ''}
            </div>
            <div style={{ fontSize: '18px' }}>
                {channelValue.Papp !== undefined && channelValue.Preal !== undefined
                ? `PF=${(channelValue.Preal/channelValue.Papp).toFixed(3)}`
                : ''}
            </div>
            <div style={{ fontSize: '18px' }}>
                {channelValue.Eact !== undefined
                ? `Eact=${(channelValue.Eact).toFixed(3)} Wh`
                : ''}
            </div>
            <div style={{ fontSize: '18px' }}>
                {channelValue.Ereact !== undefined
                ? `Ereact=${(channelValue.Ereact).toFixed(3)} Wh`
                : ''}
            </div>
          </>
          }
        </div>
      ))}
    </div>
  );
}

export default ChannelValuesDisplay;
