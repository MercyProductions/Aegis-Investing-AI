export function normalizeSymbol(symbol: string) {
  return symbol.trim().toUpperCase().replace(/\//g, '-');
}
