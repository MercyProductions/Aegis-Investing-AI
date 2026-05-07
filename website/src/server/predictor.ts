import type { PredictionResult } from './types.js';

export class PricePredictor {
  private models = new Map<string, { slope: number; intercept: number }>();

  predictPrice(symbol: string, historicalData: any[]): PredictionResult | null {
    if (historicalData.length < 10) {
      return null;
    }

    let model = this.models.get(symbol);
    if (!model) {
      model = this.trainSimpleModel(historicalData);
      this.models.set(symbol, model);
    }

    const lastPrice = historicalData[historicalData.length - 1].price;
    const predictedPrice = model.slope * historicalData.length + model.intercept;

    const direction = predictedPrice > lastPrice * 1.001 ? 'up' :
                     predictedPrice < lastPrice * 0.999 ? 'down' : 'sideways';

    return {
      symbol,
      predictedPrice,
      confidence: 0.6,
      direction,
      timestamp: Date.now()
    };
  }

  private trainSimpleModel(data: any[]): { slope: number; intercept: number } {
    // Simple linear regression on price over time
    const n = data.length;
    const sumX = (n * (n - 1)) / 2;
    const sumY = data.reduce((sum, d, i) => sum + d.price, 0);
    const sumXY = data.reduce((sum, d, i) => sum + i * d.price, 0);
    const sumXX = (n * (n - 1) * (2 * n - 1)) / 6;

    const slope = (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX);
    const intercept = (sumY - slope * sumX) / n;

    return { slope, intercept };
  }

  async getPredictions(symbols: string[], historicalData: Map<string, any[]>): Promise<Map<string, PredictionResult>> {
    const results = new Map<string, PredictionResult>();

    for (const symbol of symbols) {
      const data = historicalData.get(symbol);
      if (data) {
        const prediction = this.predictPrice(symbol, data);
        if (prediction) {
          results.set(symbol, prediction);
        }
      }
    }

    return results;
  }
}