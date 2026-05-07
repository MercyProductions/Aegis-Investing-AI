// import axios from 'axios';

export interface SentimentData {
  symbol: string;
  score: number; // -1 to 1, negative = bearish, positive = bullish
  confidence: number; // 0 to 1
  sources: string[];
  timestamp: number;
}

export class SentimentAnalyzer {
  private cache = new Map<string, SentimentData>();
  private readonly CACHE_TTL = 5 * 60 * 1000; // 5 minutes

  async getSentiment(symbol: string): Promise<SentimentData | null> {
    // Check cache first
    const cached = this.cache.get(symbol);
    if (cached && Date.now() - cached.timestamp < this.CACHE_TTL) {
      return cached;
    }

    try {
      // Use a free sentiment API or simulate
      // For demo, we'll simulate based on recent price action
      const sentiment = this.fetchSentimentFromAPI(symbol);
      this.cache.set(symbol, sentiment);
      return sentiment;
    } catch (error) {
      console.error(`Failed to fetch sentiment for ${symbol}:`, error);
      return null;
    }
  }

  private fetchSentimentFromAPI(symbol: string): SentimentData {
    // In production, use APIs like:
    // - Alpha Vantage News Sentiment
    // - Twitter API with NLP
    // - LunarCrush for crypto social sentiment

    // For now, simulate with random but realistic data
    const baseScore = (Math.random() - 0.5) * 0.8; // -0.4 to 0.4
    const momentum = Math.random() > 0.5 ? 0.1 : -0.1; // Slight bias
    const score = Math.max(-1, Math.min(1, baseScore + momentum));

    return {
      symbol,
      score,
      confidence: 0.6 + Math.random() * 0.3, // 0.6-0.9
      sources: ['Twitter', 'Reddit', 'News'],
      timestamp: Date.now()
    };
  }

  async getBulkSentiment(symbols: string[]): Promise<Map<string, SentimentData>> {
    const results = new Map<string, SentimentData>();
    await Promise.all(symbols.map(async (symbol) => {
      const sentiment = await this.getSentiment(symbol);
      if (sentiment) {
        results.set(symbol, sentiment);
      }
    }));
    return results;
  }
}