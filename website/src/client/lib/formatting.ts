export function money(value: number) {
  return new Intl.NumberFormat('en-US', { style: 'currency', currency: 'USD', maximumFractionDigits: value > 1000 ? 0 : 2 }).format(value);
}

export function pct(value: number) {
  return `${(value * 100).toFixed(2)}%`;
}

export function compact(value: number) {
  return new Intl.NumberFormat('en-US', { notation: 'compact', maximumFractionDigits: 1 }).format(value);
}
