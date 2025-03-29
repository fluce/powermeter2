export interface RawChannelData {
  time: number;
  data: number;
}

export interface ChannelValues {
  channel: number;
  Vrms?: number;
  Irms?: number;
  Papp?: number;
  Preal?: number;
  Eact?: number;
  Ereact?: number;
  unit?: string;
  time?: number;
  samplesCount?: number;
  mean?: number;
}

export interface LogState {
    logs: string[];
    buffer: string[];
    isReadingBuffer: boolean;
    channelsData: Array<Array<RawChannelData>>;
    channelsValues: Array<ChannelValues>;
    channelTypes?: Record<number, 'Voltage' | 'Current'>;
}

