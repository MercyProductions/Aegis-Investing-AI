import { execFile } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { promisify } from 'node:util';

const execFileAsync = promisify(execFile);

export interface NativeCoreStatus {
  ok: boolean;
  app: 'AuralithCore';
  mode: string;
  database: string;
  status: string;
  providerSource: string;
  providerFreshness: string;
  quoteCount: number;
  providerConfidence: number;
  rows: number;
}

let cached: { expiresAt: number; status: NativeCoreStatus | null } = { expiresAt: 0, status: null };

export async function readNativeCoreStatus(force = false): Promise<NativeCoreStatus | null> {
  if (!force && cached.expiresAt > Date.now()) return cached.status;
  const exe = resolveCoreExe();
  if (!exe) {
    cached = { expiresAt: Date.now() + 30_000, status: null };
    return null;
  }

  try {
    const { stdout } = await execFileAsync(exe, ['--once'], {
      timeout: 3500,
      windowsHide: true,
      maxBuffer: 1024 * 256
    });
    const parsed = JSON.parse(stdout.trim()) as NativeCoreStatus;
    cached = { expiresAt: Date.now() + 30_000, status: parsed };
    return parsed;
  } catch {
    cached = { expiresAt: Date.now() + 15_000, status: null };
    return null;
  }
}

function resolveCoreExe() {
  const explicit = process.env.AURALITH_CORE_EXE;
  const here = path.dirname(fileURLToPath(import.meta.url));
  const candidates = [
    explicit,
    path.resolve(process.cwd(), '..', 'x64', 'Release', 'AuralithCore.exe'),
    path.resolve(process.cwd(), 'x64', 'Release', 'AuralithCore.exe'),
    path.resolve(here, '..', '..', '..', 'x64', 'Release', 'AuralithCore.exe'),
    path.resolve(here, '..', '..', '..', '..', 'x64', 'Release', 'AuralithCore.exe')
  ].filter(Boolean) as string[];

  return candidates.find((candidate) => fs.existsSync(candidate)) ?? null;
}
