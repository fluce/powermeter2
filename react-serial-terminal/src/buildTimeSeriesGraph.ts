import * as d3 from 'd3';

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

export default buildTimeSeriesGraph;
