import React from 'react';
const root = (kind) => ({ data, children }) => <svg data-chart={kind} data-points={Array.isArray(data) ? data.length : 0}>{children}</svg>;
export const ResponsiveContainer = ({ children }) => <div data-chart-container="">{children}</div>;
export const LineChart = root('line'); export const BarChart = root('bar'); export const PieChart = root('pie');
export const Line = ({ dataKey }) => <g data-series={dataKey} />; export const Bar = ({ dataKey }) => <g data-series={dataKey} />;
export const Pie = ({ data, children }) => <g data-slices={Array.isArray(data) ? data.length : 0}>{children}</g>;
export const Cell = () => null; export const XAxis = () => null; export const YAxis = () => null; export const Tooltip = () => null; export const CartesianGrid = () => null; export const Legend = () => null;
