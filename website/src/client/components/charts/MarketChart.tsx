import { useEffect, useRef, useState } from 'react';
import {
  CandlestickSeries,
  ColorType,
  createChart,
  HistogramSeries,
  LineStyle,
  type IChartApi,
  type ISeriesApi,
  type UTCTimestamp
} from 'lightweight-charts';
import { getCandles } from '../../lib/api';
import type { CandleRecord, MarketAsset, Signal } from '../../types/api';

export function MarketChart({ asset, signal }: { asset: MarketAsset | null; signal?: Signal | null }) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const candleSeriesRef = useRef<ISeriesApi<'Candlestick'> | null>(null);
  const volumeSeriesRef = useRef<ISeriesApi<'Histogram'> | null>(null);
  const [candles, setCandles] = useState<CandleRecord[]>([]);
  const [status, setStatus] = useState('Loading chart history...');

  useEffect(() => {
    if (!asset) return;
    let cancelled = false;
    setStatus('Loading chart history...');
    getCandles(asset.symbol)
      .then((result) => {
        if (cancelled) return;
        setCandles(result.candles);
        setStatus(result.candles.some((candle) => candle.synthetic) ? 'Synthetic/fallback candle cache' : 'Provider candle cache');
      })
      .catch((error) => {
        if (cancelled) return;
        setCandles([]);
        setStatus(error instanceof Error ? error.message : 'Could not load chart candles.');
      });
    return () => {
      cancelled = true;
    };
  }, [asset?.symbol]);

  useEffect(() => {
    const container = containerRef.current;
    if (!container) return;
    const chart = createChart(container, {
      autoSize: true,
      layout: {
        background: { type: ColorType.Solid, color: '#090d13' },
        textColor: '#9aa7b5'
      },
      grid: {
        vertLines: { color: 'rgba(255,255,255,0.045)' },
        horzLines: { color: 'rgba(255,255,255,0.045)' }
      },
      rightPriceScale: {
        borderColor: 'rgba(255,255,255,0.08)'
      },
      timeScale: {
        borderColor: 'rgba(255,255,255,0.08)',
        timeVisible: true
      },
      crosshair: {
        mode: 0
      }
    });
    const candleSeries = chart.addSeries(CandlestickSeries, {
      upColor: '#7FB2FF',
      downColor: '#FF6B6B',
      borderUpColor: '#7FB2FF',
      borderDownColor: '#FF6B6B',
      wickUpColor: '#9EC5FF',
      wickDownColor: '#FF8D8D'
    });
    const volumeSeries = chart.addSeries(HistogramSeries, {
      priceFormat: { type: 'volume' },
      priceScaleId: '',
      color: 'rgba(127,178,255,0.25)'
    });
    volumeSeries.priceScale().applyOptions({
      scaleMargins: {
        top: 0.78,
        bottom: 0
      }
    });
    chartRef.current = chart;
    candleSeriesRef.current = candleSeries;
    volumeSeriesRef.current = volumeSeries;
    return () => {
      chart.remove();
      chartRef.current = null;
      candleSeriesRef.current = null;
      volumeSeriesRef.current = null;
    };
  }, []);

  useEffect(() => {
    const candleSeries = candleSeriesRef.current;
    const volumeSeries = volumeSeriesRef.current;
    if (!candleSeries || !volumeSeries || !candles.length) return;
    candleSeries.setData(candles.map((candle) => ({
      time: Math.floor(candle.timestamp / 1000) as UTCTimestamp,
      open: candle.open,
      high: candle.high,
      low: candle.low,
      close: candle.close
    })));
    volumeSeries.setData(candles.map((candle) => ({
      time: Math.floor(candle.timestamp / 1000) as UTCTimestamp,
      value: candle.volume,
      color: candle.close >= candle.open ? 'rgba(127,178,255,0.28)' : 'rgba(255,107,107,0.22)'
    })));
    candleSeries.priceLines().forEach((line) => candleSeries.removePriceLine(line));
    if (signal) {
      candleSeries.createPriceLine({ price: signal.entry, color: '#7FB2FF', lineWidth: 1, lineStyle: LineStyle.Dashed, title: 'Entry' });
      candleSeries.createPriceLine({ price: signal.stop, color: '#FF6B6B', lineWidth: 1, lineStyle: LineStyle.Dashed, title: 'Stop' });
      candleSeries.createPriceLine({ price: signal.target1, color: '#A7C957', lineWidth: 1, lineStyle: LineStyle.Dashed, title: 'Target 1' });
      candleSeries.createPriceLine({ price: signal.target2, color: '#E6C35C', lineWidth: 1, lineStyle: LineStyle.Dashed, title: 'Target 2' });
    }
    chartRef.current?.timeScale().fitContent();
  }, [candles, signal]);

  return (
    <div className="market-chart-shell">
      <div className="chart-meta-row">
        <span>{asset?.symbol ?? 'Select symbol'} / 1D OHLCV</span>
        <strong>{status}</strong>
      </div>
      <div className="market-chart" ref={containerRef} />
    </div>
  );
}
