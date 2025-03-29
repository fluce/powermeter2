import * as d3 from 'd3';
import React from 'react';
import { RawChannelData } from './types';

function buildTimeSeriesGraph(
  channelsData: Array<Array<RawChannelData>>,
  svgElement: SVGSVGElement | null,
  visibleChannels: Record<number, boolean>,
  setCursorData: React.Dispatch<React.SetStateAction<Record<number, RawChannelData>>>
) {
  if (svgElement && channelsData.length > 0) {
    const svg = d3.select(svgElement);
    svg.selectAll("*").remove();

    const width = 800;
    const height = 400;
    const margin = { top: 20, right: 30, bottom: 30, left: 40 };

    const x = d3.scaleLinear()
      .domain(d3.extent(channelsData.flat(), d => d.time) as [number, number])
      .range([margin.left, width - margin.right]);

    const y = d3.scaleLinear()
      .domain([d3.min(channelsData.flat(), d => d.data) || 0, d3.max(channelsData.flat(), d => d.data) || 0])
      .nice()
      .range([height - margin.bottom, margin.top]);

    const line = d3.line<RawChannelData>()
      .x(d => x(d.time))
      .y(d => y(d.data));

    const zoom = d3.zoom<SVGSVGElement, unknown>()
      .scaleExtent([1, 500])
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

        // Add dots for each measure point
        lineGroup.selectAll(`.dot-${index}`)
          .data(channelData)
          .enter()
          .append("circle")
          .attr("class", `dot-${index}`)
          .attr("cx", d => x(d.time))
          .attr("cy", d => y(d.data))
          .attr("r", 3)
          .attr("fill", d3.schemeCategory10[index % 10])
          .style("display", "none"); // Hide dots by default
      }
    });

    const cursorLine = svg.append("line")
      .attr("stroke", "black")
      .attr("stroke-width", 1)
      .attr("y1", margin.top)
      .attr("y2", height - margin.bottom)
      .style("display", "none");

    const cursorText = svg.append("text")
      .attr("text-anchor", "start")
      .attr("font-size", "12px")
      .attr("fill", "black")
      .style("display", "none");

    svg.on("mousemove", (event) => {
      const [mouseX] = d3.pointer(event);
      const zoomTransform = d3.zoomTransform(svgElement as SVGSVGElement); // Get current zoom transform
      const scaledX = zoomTransform.rescaleX(x); // Apply zoom scaling to x-axis
      const time = scaledX.invert(mouseX); // Use scaled x-axis for time calculation

      const newCursorData = channelsData.map((channelData) => {
        const closestPoint = channelData.reduce((prev, curr) => (
          Math.abs(curr.time - time) < Math.abs(prev.time - time) ? curr : prev
        ));
        return closestPoint;
      });

      setCursorData(Object.fromEntries(newCursorData.map((data, index) => [index, data]).filter(x => x !== null)));
      cursorLine
        .attr("x1", mouseX)
        .attr("x2", mouseX)
        .style("display", null);

      cursorText
        .attr("x", mouseX + 5)
        .attr("y", margin.top)
        .text(`Time: ${time.toFixed(2)}`)
        .style("display", null);
    });

    svg.on("mouseleave", () => {
      cursorLine.style("display", "none");
      cursorText.style("display", "none");
      setCursorData({});
    });

    svg.call(zoom);
    function zoomed(event: d3.D3ZoomEvent<SVGSVGElement, unknown>) {
      const newX = event.transform.rescaleX(x);
      const zoomLevel = event.transform.k;

      (svg.select(".x-axis") as d3.Selection<SVGGElement, unknown, null, undefined>).call(d3.axisBottom(newX));

      lineGroup.selectAll("path")
        .attr("d", (d) => d3.line<RawChannelData>()
          .x(d => newX(d.time))
          .y(d => y(d.data))(d as RawChannelData[]) || ""
        );

      // Update dots during zoom
      lineGroup.selectAll("circle")
        .attr("cx", d => newX((d as RawChannelData).time))
        .attr("cy", d => y((d as RawChannelData).data))
        .style("display", zoomLevel > 20 ? "initial" : "none");
    }
  }
}

export default buildTimeSeriesGraph;
