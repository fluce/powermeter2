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

function App() {
  const [port, setPort] = useState(null);
  const [reader, setReader] = useState(null);
  const [logs, setLogs] = useState({ logs: [], buffer: [], isReadingBuffer: false, lastLine: "", channelsData: [] });
  const [error, setError] = useState(null);
  const svgRef = useRef();
  const [visibleChannels, setVisibleChannels] = useState({});

  useEffect(() => {
    if (port && reader) {
      const readLoop = async () => {
        while (true) {
          const { value, done } = await reader.read();
          if (done) {
            break;
          }
          const hexValue = Array.from(value).map(byte => byte.toString(16).padStart(2, '0'));
          const stringLines = String.fromCharCode(...value).split(/(\r\n|\n)/).filter(line => line !== "\r\n" && line !== "\n");
          setLogs((prevLogs) => {
            const lastLog = prevLogs.lastLine;
            const newLogs = [(lastLog ?? "") + stringLines[0], ...stringLines.slice(1)];
            let isReadingBuffer = prevLogs.isReadingBuffer;
            const addedToLogs = [];
            const addedToBuffer = [];
            let buffer = prevLogs.buffer;
            let bufferComplete = false;
            for (let i = 0; i < newLogs.length - 1; i++) {
              if (isReadingBuffer) {
                if (newLogs[i].startsWith("Channel")) {
                  bufferComplete = true;
                  isReadingBuffer = false;
                }
              } else {
                if (newLogs[i].startsWith("Buffer full")) {
                  isReadingBuffer = true;
                  buffer = [];
                  continue;
                }
              }
              if (isReadingBuffer) {
                addedToBuffer.push(newLogs[i]);
              } else {
                addedToLogs.push(newLogs[i]);
              }
            }
            const res = {
              logs: [...prevLogs.logs, ...addedToLogs],
              buffer: bufferComplete ? [] : [...buffer, ...addedToBuffer],
              isReadingBuffer,
              lastLine: newLogs[newLogs.length - 1],
              channelsData: bufferComplete ? convert(buffer) : prevLogs.channelsData
            };
            return res;
          });
        }
      };
      readLoop();
    }
  }, [port, reader]);

  useEffect(() => {
    if (logs.channelsData.length > 0) {
      const svg = d3.select(svgRef.current);
      svg.selectAll("*").remove();

      const width = 800;
      const height = 400;
      const margin = { top: 20, right: 30, bottom: 30, left: 40 };

      const x = d3.scaleLinear()
        .domain(d3.extent(logs.channelsData.flat(), d => d.time))
        .range([margin.left, width - margin.right]);

      const y = d3.scaleLinear()
        .domain([d3.min(logs.channelsData.flat(), d => d.data), d3.max(logs.channelsData.flat(), d => d.data)])
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

      logs.channelsData.forEach((channelData, index) => {
        if (visibleChannels[index] !== false) {
          lineGroup.append("path")
            .datum(channelData)
            .attr("fill", "none")
            .attr("stroke", d3.schemeCategory10[index % 10])
            .attr("stroke-width", 1.5)
            .attr("d", line);
        }
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
  }, [logs.channelsData, visibleChannels]);

  const connectSerial = async () => {
    try {
      const port = await navigator.serial.requestPort();
      await port.open({ baudRate: 9600 });
      const reader = port.readable.getReader();
      setPort(port);
      setReader(reader);
      setError(null);
    } catch (err) {
      setError(`Failed to open serial port: ${err.message}`);
    }
  };

  const handleCheckboxChange = (index) => {
    setVisibleChannels((prev) => ({
      ...prev,
      [index]: !prev[index]
    }));
  };

  return (
    <div>
      <button onClick={connectSerial}>Connect to Serial Port</button>
      {error && <div style={{ color: 'red' }}>{error}</div>}
      <div style={{ backgroundColor: 'black', color: 'white', padding: '10px', height: '400px', overflowY: 'scroll' }}>
        {logs.logs.map((log, index) => (
          <div key={index}>{log}</div>
        ))}
      </div>
      <svg ref={svgRef} width="800" height="400" style={{ backgroundColor: 'lightgray' }}></svg>
      <div>
        {logs.channelsData.map((_, index) => (
          <div key={index}>
            <input
              type="checkbox"
              checked={visibleChannels[index] !== false}
              onChange={() => handleCheckboxChange(index)}
            />
            <label>Channel {index}</label>
          </div>
        ))}
      </div>
    </div>
  );
}

export default App;
