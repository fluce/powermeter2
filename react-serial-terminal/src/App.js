import React, { useState, useEffect, useRef } from 'react';
import * as d3 from 'd3';

function convert(buffer) {
  const channelsData = [];
  let time = 0;
  for (let i = 0; i < buffer.length; i++) {
    const line = buffer[i];
    const fields = line.split(" ");
    const dt = fields[1];
    time += parseInt(dt);
    const channel = fields[2];
    const data = parseInt(fields[3]);
    const tab = channelsData[channel] ?? [];
    tab.push({ time, data });
    channelsData[channel] = tab;
  }
  return channelsData;
}

function* reverse(arr, maxItems) {
  for (let i = arr.length - 1; i >= 0 && maxItems > 0; i--, maxItems--) {
    yield arr[i];
  }
}
function App() {
  const [port, setPort] = useState(null);
  const [reader, setReader] = useState(null);
  const [streamClosed, setStreamClosed] = useState(null);
  const [logs, setLogs] = useState({ logs: [], buffer: [], isReadingBuffer: false, lastLine: "", channelsData: [], channelsValues: [] });
  const [error, setError] = useState(null);
  const svgRef = useRef();
  const [visibleChannels, setVisibleChannels] = useState({});
  const [cursorData, setCursorData] = useState({});

  useEffect(() => {
    if (port && reader) {
      const readLoop = async () => {
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
              let channelsValues = prevLogs.channelsValues
              if (addedToLogs.length>0) {
                const lineData = Object.fromEntries(
                                    newLine.split("\t")
                                           .map(x=>x.split("=",2))
                                           .map(([k,v])=>[k,isNaN(v)?v:parseFloat(v)])
                                  );                
                if (lineData.Channel!==undefined)
                  channelsValues[lineData.Channel] = {
                    channel: lineData.Channel,
                    Vrms: lineData.Vrms,
                    Irms: lineData.Irms,
                    unit: lineData.Unit,
                    time: lineData.Time,
                    samplesCount: lineData.SamplesCount,
                    mean: lineData.Mean
                  };
              }
              const res = {
                logs: [...prevLogs.logs, ...addedToLogs].slice(-101),
                buffer: bufferComplete ? [] : [...buffer, ...addedToBuffer],
                isReadingBuffer,
                channelsData: bufferComplete ? convert(buffer) : prevLogs.channelsData,
                channelsValues
              };
              return res;
            });
          }
        } finally {
          reader.releaseLock();
        }
      };
      readLoop();
    }
  }, [port, reader]);

  useEffect(() => {
    buildTimeSeriesGraph(logs.channelsData, svgRef, visibleChannels, setCursorData);
  }, [logs.channelsData, visibleChannels]);

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

  const handleCheckboxChange = (index) => {
    setVisibleChannels((prev) => ({
      ...prev,
      [index]: !prev[index]
    }));
  };

  return (
    <div>
      <button onClick={connectSerial} disabled={port!==null}>Connect to Serial Port</button>
      <button onClick={disconnectSerial} disabled={!port}>Disconnect</button>
      {error && <div style={{ color: 'red' }}>{error}</div>}
      <div style={{ backgroundColor: 'black', color: 'white', padding: '10px', height: '400px', overflowY: 'scroll' }}>
        {[...reverse(logs.logs,100).map((log, index) => (
          <div key={index}>{log}</div>
        ))]}
      </div>
      <div style={{ display: 'flex' }}>
        <svg ref={svgRef} width="800" height="400" style={{ backgroundColor: 'lightgray' }}></svg>
        <div style={{ display: 'flex', flexWrap: 'wrap', marginLeft: '10px' }}>
          {logs.channelsValues.map((channelValue, index) => (
            <div key={index} style={{ border: '1px solid black', padding: '10px', margin: '5px', textAlign: 'center' }}>
              <div style={{ fontSize: '12px' }}>Channel {channelValue.channel}</div>
              <div style={{ fontSize: '24px' }}>
                {channelValue.Irms !== undefined ? `${channelValue.Irms} ${channelValue.unit}` : `${channelValue.Vrms} ${channelValue.unit}`}
              </div>
            </div>
          ))}
        </div>
      </div>
      <div>
        {logs.channelsData.map((_, index) => (
          <div key={index}>
            <input
              type="checkbox"
              checked={visibleChannels[index] !== false}
              onChange={() => handleCheckboxChange(index)}
            />
            <label>Channel {index} {cursorData[index] ? `: ${cursorData[index].data}` : ''}</label>
          </div>
        ))}
      </div>
    </div>
  );
}

class LineBreakTransformer {
  constructor() {
    this.container = '';
  }

  transform(chunk, controller) {
    this.container += chunk;
    const lines = this.container.split('\r\n');
    this.container = lines.pop();
    lines.forEach(line => controller.enqueue(line));
  }

  flush(controller) {
    controller.enqueue(this.container);
  }
}

export default App;

function buildTimeSeriesGraph(channelsData, svgRef, visibleChannels, setCursorData) {
  if (channelsData.length > 0) {
    const svg = d3.select(svgRef.current);
    svg.selectAll("*").remove();

    const width = 800;
    const height = 400;
    const margin = { top: 20, right: 30, bottom: 30, left: 40 };

    const x = d3.scaleLinear()
      .domain(d3.extent(channelsData.flat(), d => d.time))
      .range([margin.left, width - margin.right]);

    const y = d3.scaleLinear()
      .domain([d3.min(channelsData.flat(), d => d.data), d3.max(channelsData.flat(), d => d.data)])
      .nice()
      .range([height - margin.bottom, margin.top]);

    const line = d3.line()
      .x(d => x(d.time))
      .y(d => y(d.data));

    const zoom = d3.zoom()
      .scaleExtent([1, 30])
      .translateExtent([[0, 0], [width, height]])
      .extent([[0, 0], [width, height]])
      .on("zoom", zoomed);

    svg.append("g")
      .attr("transform", `translate(0,${height - margin.bottom})`)
      .call(d3.axisBottom(x))
      .attr("class", "x-axis");

    svg.append("g")
      .attr("transform", `translate(${margin.left},0)`)
      .call(d3.axisLeft(y))
      .attr("class", "y-axis");

    const lineGroup = svg.append("g");

    channelsData.forEach((channelData, index) => {
      if (visibleChannels[index] !== false) {
        lineGroup.append("path")
          .datum(channelData)
          .attr("fill", "none")
          .attr("stroke", d3.schemeCategory10[index % 10])
          .attr("stroke-width", 1.5)
          .attr("d", line);
      }
    });

    const cursorLine = svg.append("line")
      .attr("stroke", "black")
      .attr("stroke-width", 1)
      .attr("y1", margin.top)
      .attr("y2", height - margin.bottom)
      .style("display", "none");

    svg.on("mousemove", (event) => {
      const [mouseX] = d3.pointer(event);
      const time = x.invert(mouseX);
      const newCursorData = channelsData.map((channelData) => {
        const closestPoint = channelData.reduce((prev, curr) => (
          Math.abs(curr.time - time) < Math.abs(prev.time - time) ? curr : prev
        ));
        return closestPoint;
      });
      setCursorData(newCursorData);
      cursorLine
        .attr("x1", mouseX)
        .attr("x2", mouseX)
        .style("display", null);
    });

    svg.on("mouseleave", () => {
      cursorLine.style("display", "none");
      setCursorData({});
    });

    svg.call(zoom);

    function zoomed(event) {
      const newX = event.transform.rescaleX(x);

      svg.select(".x-axis").call(d3.axisBottom(newX));

      lineGroup.selectAll("path")
        .attr("d", d3.line()
          .x(d => newX(d.time))
          .y(d => y(d.data))
        );
    }
  }
}

