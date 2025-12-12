import { Dialog, DialogTitle, DialogContent, IconButton, Box, Typography, Chip, Stack, Grid } from '@mui/material';
import { Close, GraphicEq, VolumeOff, Waves, MusicNote, Computer, Memory } from '@mui/icons-material';
import ReactECharts from 'echarts-for-react';
import { useState, useEffect } from 'react';

function MetricsHistory({ open, onClose, history, modelChanges, bypassChanges, status }) {
  const [systemInfo, setSystemInfo] = useState(null);
  const [renderChart, setRenderChart] = useState(false);
  const [chartKey, setChartKey] = useState(0);

  useEffect(() => {
    if (open) {
      // Fetch system info when modal opens
      fetch('/api/system')
        .then(res => res.json())
        .then(data => {
          if (data.success) {
            setSystemInfo(data);
          }
        })
        .catch(err => console.error('Error fetching system info:', err));

      // Delay chart rendering to avoid resize observer errors during modal open animation
      setRenderChart(false);
      const timer = setTimeout(() => {
        setChartKey(prev => prev + 1);
        setRenderChart(true);
      }, 100);
      return () => clearTimeout(timer);
    } else {
      setRenderChart(false);
    }
  }, [open]);
  const getOption = () => {
    if (!history || history.length === 0) {
      return {};
    }

    // Extract data for each metric
    const timestamps = history.map(h => new Date(h.timestamp));
    const cpuLoad = history.map(h => h.cpuLoad);
    const cpuTemp = history.map(h => h.cpuTemp >= 0 ? h.cpuTemp : null);
    const memoryUsage = history.map(h => h.memoryUsage >= 0 ? h.memoryUsage : null);
    const xruns = history.map(h => h.xruns);

    // Prepare model change annotations with before/after labels
    const modelChangeMarkers = [];
    modelChanges.forEach(change => {
      // Add three entries: before label, line, after label

      // "Before" label on the left
      modelChangeMarkers.push({
        xAxis: change.timestamp,
        label: {
          show: true,
          formatter: change.previousModel,
          position: 'insideStartTop',
          fontSize: 10,
          color: '#ff9800',
          backgroundColor: 'rgba(0,0,0,0.8)',
          padding: [3, 6],
          borderRadius: 3,
          offset: [-10, 0],
        },
        lineStyle: {
          color: 'transparent',
          width: 0,
        },
      });

      // Main vertical line
      modelChangeMarkers.push({
        xAxis: change.timestamp,
        label: {
          show: true,
          formatter: '▼',
          position: 'insideEndTop',
          fontSize: 16,
          color: '#90caf9',
          offset: [0, 10],
        },
        lineStyle: {
          color: '#90caf9',
          type: 'dashed',
          width: 2,
        },
      });

      // "After" label on the right
      modelChangeMarkers.push({
        xAxis: change.timestamp,
        label: {
          show: true,
          formatter: change.newModel,
          position: 'insideStartBottom',
          fontSize: 10,
          color: '#4caf50',
          backgroundColor: 'rgba(0,0,0,0.8)',
          padding: [3, 6],
          borderRadius: 3,
          offset: [10, 0],
        },
        lineStyle: {
          color: 'transparent',
          width: 0,
        },
      });
    });

    // Prepare NAM bypass change annotations
    const bypassChangeMarkers = [];
    if (bypassChanges) {
      bypassChanges.forEach(change => {
        const label = change.bypassed
          ? `NAM Bypassed: ${change.modelName}`
          : `NAM Active: ${change.modelName}`;
        const color = change.bypassed ? '#ff9800' : '#4caf50';

        // Single marker with label
        bypassChangeMarkers.push({
          xAxis: change.timestamp,
          label: {
            show: true,
            formatter: label,
            position: 'insideEndBottom',
            fontSize: 10,
            color: color,
            backgroundColor: 'rgba(0,0,0,0.8)',
            padding: [3, 6],
            borderRadius: 3,
            offset: [0, -10],
          },
          lineStyle: {
            color: color,
            type: 'solid',
            width: 2,
          },
        });
      });
    }

    return {
      title: {
        text: 'HoopiPi System Performance',
        left: 'center',
        textStyle: {
          color: '#ffffff',
        },
      },
      tooltip: {
        trigger: 'axis',
        axisPointer: {
          type: 'cross',
        },
        backgroundColor: 'rgba(30, 30, 30, 0.9)',
        borderColor: '#90caf9',
        textStyle: {
          color: '#ffffff',
        },
        formatter: (params) => {
          if (!params || params.length === 0) return '';

          let result = `${params[0].axisValueLabel}<br/>`;
          params.forEach(param => {
            const value = param.value[1];
            if (value === null || value === undefined) return;

            // Show xruns as integer, others with 1 decimal
            const formattedValue = param.seriesName === 'Xruns'
              ? Math.round(value)
              : Number(value).toFixed(1);

            // Use colored text instead of marker circles
            const color = param.color;
            result += `<span style="color:${color}">■</span> ${param.seriesName}: ${formattedValue}<br/>`;
          });
          return result;
        },
      },
      legend: {
        data: ['CPU Load (%)', 'CPU Temp (°C)', 'Memory (MB)', 'Xruns'],
        bottom: 10,
        textStyle: {
          color: '#ffffff',
        },
        icon: 'rect',  // Use rectangles instead of circles
      },
      grid: {
        left: '3%',
        right: '4%',
        bottom: '15%',
        top: '15%',
        containLabel: true,
      },
      xAxis: {
        type: 'time',
        boundaryGap: false,
        axisLabel: {
          color: '#aaaaaa',
          formatter: (value) => {
            const date = new Date(value);
            return `${date.getHours().toString().padStart(2, '0')}:${date.getMinutes().toString().padStart(2, '0')}`;
          },
        },
        axisLine: {
          lineStyle: {
            color: '#555555',
          },
        },
      },
      yAxis: [
        {
          type: 'value',
          name: 'CPU / Memory / Temp',
          position: 'left',
          axisLabel: {
            color: '#aaaaaa',
          },
          axisLine: {
            lineStyle: {
              color: '#555555',
            },
          },
          splitLine: {
            lineStyle: {
              color: '#333333',
            },
          },
        },
        {
          type: 'value',
          name: 'Xruns',
          position: 'right',
          axisLabel: {
            color: '#aaaaaa',
          },
          axisLine: {
            lineStyle: {
              color: '#555555',
            },
          },
          splitLine: {
            show: false,
          },
        },
      ],
      series: [
        {
          name: 'CPU Load (%)',
          type: 'line',
          data: timestamps.map((t, i) => [t, cpuLoad[i]]),
          smooth: true,
          symbol: 'none',
          color: '#f48fb1',  // Set color at series level for legend/tooltip
          lineStyle: {
            width: 2,
          },
          areaStyle: {
            color: {
              type: 'linear',
              x: 0,
              y: 0,
              x2: 0,
              y2: 1,
              colorStops: [
                { offset: 0, color: 'rgba(244, 143, 177, 0.3)' },
                { offset: 1, color: 'rgba(244, 143, 177, 0.0)' },
              ],
            },
          },
          yAxisIndex: 0,
          markLine: {
            silent: true,
            data: [...modelChangeMarkers, ...bypassChangeMarkers],
          },
        },
        {
          name: 'CPU Temp (°C)',
          type: 'line',
          data: timestamps.map((t, i) => [t, cpuTemp[i]]),
          smooth: true,
          symbol: 'none',
          color: '#ff9800',  // Set color at series level for legend/tooltip
          lineStyle: {
            width: 2,
          },
          yAxisIndex: 0,
        },
        {
          name: 'Memory (MB)',
          type: 'line',
          data: timestamps.map((t, i) => [t, memoryUsage[i]]),
          smooth: true,
          symbol: 'none',
          color: '#4caf50',  // Set color at series level for legend/tooltip
          lineStyle: {
            width: 2,
          },
          yAxisIndex: 0,
        },
        {
          name: 'Xruns',
          type: 'line',
          data: timestamps.map((t, i) => [t, xruns[i]]),
          step: 'end',
          symbol: 'none',
          color: '#f44336',  // Set color at series level for legend/tooltip
          lineStyle: {
            width: 2,
          },
          yAxisIndex: 1,
        },
      ],
    };
  };

  return (
    <Dialog
      open={open}
      onClose={onClose}
      maxWidth="lg"
      fullWidth
      PaperProps={{
        sx: {
          bgcolor: 'background.paper',
          minHeight: '60vh',
        },
      }}
    >
      <DialogTitle>
        <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <Typography variant="h5">HoopiPi System Performance</Typography>
          <IconButton onClick={onClose} size="small">
            <Close />
          </IconButton>
        </Box>
      </DialogTitle>
      <DialogContent>
        {/* System Information */}
        {systemInfo && (
          <Box sx={{ mb: 2, p: 2, bgcolor: 'background.default', borderRadius: 1 }}>
            <Typography variant="subtitle2" sx={{ mb: 1, color: 'text.secondary' }}>
              System Information
            </Typography>
            <Grid container spacing={2}>
              <Grid item xs={12} sm={6} md={4}>
                <Typography variant="caption" display="block" color="text.secondary">
                  Model
                </Typography>
                <Typography variant="body2" sx={{ fontWeight: 'bold' }}>
                  {systemInfo.piModel}
                </Typography>
              </Grid>
              <Grid item xs={12} sm={6} md={4}>
                <Typography variant="caption" display="block" color="text.secondary">
                  CPU Usage
                </Typography>
                <Typography variant="body2" sx={{ fontWeight: 'bold' }}>
                  {status?.processCpu?.toFixed(1) || 0}%
                </Typography>
              </Grid>
              <Grid item xs={12} sm={6} md={4}>
                <Typography variant="caption" display="block" color="text.secondary">
                  Memory
                </Typography>
                <Typography variant="body2" sx={{ fontWeight: 'bold' }}>
                  {status?.memoryUsage?.toFixed(0) || 0} / {systemInfo.totalMemoryMB?.toFixed(0)} MB
                </Typography>
              </Grid>
              <Grid item xs={12} sm={6} md={4}>
                <Typography variant="caption" display="block" color="text.secondary">
                  HoopiPi Version
                </Typography>
                <Typography variant="body2" sx={{ fontWeight: 'bold' }}>
                  {systemInfo.packageVersion}
                </Typography>
              </Grid>
              {systemInfo.packageName !== 'Unknown' && (
                <Grid item xs={12} sm={6} md={4}>
                  <Typography variant="caption" display="block" color="text.secondary">
                    Package Name
                  </Typography>
                  <Typography variant="body2" sx={{ fontWeight: 'bold', fontSize: '0.75rem' }}>
                    {systemInfo.packageName}
                  </Typography>
                </Grid>
              )}
              {systemInfo.buildDate !== 'Unknown' && (
                <Grid item xs={12} sm={6} md={4}>
                  <Typography variant="caption" display="block" color="text.secondary">
                    Build Date
                  </Typography>
                  <Typography variant="body2" sx={{ fontWeight: 'bold', fontSize: '0.75rem' }}>
                    {systemInfo.buildDate}
                  </Typography>
                </Grid>
              )}
              {systemInfo.buildCPU !== 'Unknown' && systemInfo.buildCPU !== 'unknown' && (
                <Grid item xs={12} sm={6} md={4}>
                  <Typography variant="caption" display="block" color="text.secondary">
                    Build CPU
                  </Typography>
                  <Typography variant="body2" sx={{ fontWeight: 'bold' }}>
                    {systemInfo.buildCPU}
                  </Typography>
                </Grid>
              )}
              {systemInfo.buildFlags !== 'Unknown' && (
                <Grid item xs={12}>
                  <Typography variant="caption" display="block" color="text.secondary">
                    Build Flags
                  </Typography>
                  <Typography variant="body2" sx={{ fontWeight: 'bold', fontSize: '0.75rem', fontFamily: 'monospace' }}>
                    {systemInfo.buildFlags}
                  </Typography>
                </Grid>
              )}
              {systemInfo.jackDevice && systemInfo.jackDevice !== 'Unknown' && (
                <Grid item xs={12} sm={6} md={4}>
                  <Typography variant="caption" display="block" color="text.secondary">
                    JACK Audio Device
                  </Typography>
                  <Typography variant="body2" sx={{ fontWeight: 'bold' }}>
                    {systemInfo.jackDevice}
                    {systemInfo.jackDeviceName && ` (${systemInfo.jackDeviceName})`}
                  </Typography>
                </Grid>
              )}
              {systemInfo.jackBufferSize > 0 && (
                <Grid item xs={12} sm={6} md={4}>
                  <Typography variant="caption" display="block" color="text.secondary">
                    JACK Buffer Size
                  </Typography>
                  <Typography variant="body2" sx={{ fontWeight: 'bold' }}>
                    {systemInfo.jackBufferSize} frames
                  </Typography>
                </Grid>
              )}
            </Grid>
          </Box>
        )}

        {/* Audio Processing Status */}
        {status && (
          <Box sx={{ mb: 2, p: 2, bgcolor: 'background.default', borderRadius: 1 }}>
            <Typography variant="subtitle2" sx={{ mb: 1, color: 'text.secondary' }}>
              Audio Processing Status
            </Typography>
            <Stack direction="row" spacing={1} flexWrap="wrap" useFlexGap>
              {/* Noise Gate Status */}
              {status.stereoMode === 'Stereo2Stereo' ? (
                <>
                  <Chip
                    icon={<VolumeOff />}
                    label={`Noise Gate L: ${status.noiseGateEnabledL ? 'ON' : 'OFF'}`}
                    color={status.noiseGateEnabledL ? 'success' : 'default'}
                    size="small"
                    variant="outlined"
                  />
                  <Chip
                    icon={<VolumeOff />}
                    label={`Noise Gate R: ${status.noiseGateEnabledR ? 'ON' : 'OFF'}`}
                    color={status.noiseGateEnabledR ? 'success' : 'default'}
                    size="small"
                    variant="outlined"
                  />
                </>
              ) : (
                <Chip
                  icon={<VolumeOff />}
                  label={`Noise Gate: ${status.noiseGateEnabledL ? 'ON' : 'OFF'}`}
                  color={status.noiseGateEnabledL ? 'success' : 'default'}
                  size="small"
                  variant="outlined"
                />
              )}

              {/* NAM Model Status - Only show left channel (R is not used) */}
              <Chip
                icon={<MusicNote />}
                label={`NAM: ${status.bypassModelL ? 'BYPASSED' : (status.modelNames && status.modelNames[status.activeModelL] ? status.modelNames[status.activeModelL] : 'None')}`}
                color={status.bypassModelL ? 'warning' : 'primary'}
                size="small"
                variant="outlined"
              />

              {/* EQ Status */}
              {status.stereoMode === 'Stereo2Stereo' ? (
                <>
                  <Chip
                    icon={<GraphicEq />}
                    label={`EQ L: ${status.eqEnabledL ? 'ON' : 'OFF'}`}
                    color={status.eqEnabledL ? 'success' : 'default'}
                    size="small"
                    variant="outlined"
                  />
                  <Chip
                    icon={<GraphicEq />}
                    label={`EQ R: ${status.eqEnabledR ? 'ON' : 'OFF'}`}
                    color={status.eqEnabledR ? 'success' : 'default'}
                    size="small"
                    variant="outlined"
                  />
                </>
              ) : (
                <Chip
                  icon={<GraphicEq />}
                  label={`EQ: ${status.eqEnabledL ? 'ON' : 'OFF'}`}
                  color={status.eqEnabledL ? 'success' : 'default'}
                  size="small"
                  variant="outlined"
                />
              )}

              {/* Reverb Status */}
              <Chip
                icon={<Waves />}
                label={`Reverb: ${status.reverbEnabled ? 'ON' : 'OFF'}`}
                color={status.reverbEnabled ? 'success' : 'default'}
                size="small"
                variant="outlined"
              />
            </Stack>
          </Box>
        )}

        {renderChart && history && history.length > 0 ? (
          <ReactECharts
            key={chartKey}
            option={getOption()}
            style={{ height: '500px', width: '100%' }}
            theme="dark"
            opts={{ renderer: 'canvas' }}
          />
        ) : open && (!history || history.length === 0) ? (
          <Box sx={{ display: 'flex', justifyContent: 'center', alignItems: 'center', height: '500px' }}>
            <Typography color="text.secondary">
              No metrics data available yet. Metrics will appear as data is collected.
            </Typography>
          </Box>
        ) : null}
      </DialogContent>
    </Dialog>
  );
}

export default MetricsHistory;
