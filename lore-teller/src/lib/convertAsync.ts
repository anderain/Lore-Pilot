// Main-thread proxy for the convert worker.
//
// Falls back to running the pipeline inline when `Worker` is unavailable
// (jsdom tests, very old browsers), so callers do not need two code paths.

import { convertText, type ConvertOptions, type ConvertResult } from './convert';
import type { ConvertProgress } from './progress';
import type { WorkerResponse } from './convertWorkerProtocol';

export interface ConvertAsyncOptions extends ConvertOptions {
  onProgress?: (progress: ConvertProgress) => void;
  signal?: AbortSignal;
}

let nextId = 1;

export function workerSupported(): boolean {
  return typeof Worker !== 'undefined';
}

export function convertTextAsync(
  buffer: ArrayBuffer,
  options: ConvertAsyncOptions,
): Promise<ConvertResult> {
  const { onProgress, signal, ...rest } = options;

  if (signal?.aborted === true) {
    return Promise.reject(new DOMException('Aborted', 'AbortError'));
  }

  if (!workerSupported()) {
    return Promise.resolve().then(() => convertText(buffer, { ...rest, onProgress }));
  }

  const id = nextId++;

  return new Promise<ConvertResult>((resolve, reject) => {
    const worker = new Worker(new URL('../workers/convert.worker.ts', import.meta.url), {
      type: 'module',
    });

    let settled = false;

    const finish = () => {
      if (settled) return false;
      settled = true;
      worker.terminate();
      return true;
    };

    const onAbort = () => {
      if (finish()) reject(new DOMException('Aborted', 'AbortError'));
    };

    signal?.addEventListener('abort', onAbort, { once: true });

    worker.onmessage = (event: MessageEvent<WorkerResponse>) => {
      const message = event.data;
      if (message.id !== id) return;

      if (message.type === 'progress') {
        onProgress?.(message.progress);
        return;
      }

      if (!finish()) return;
      signal?.removeEventListener('abort', onAbort);

      if (message.type === 'error') {
        reject(new Error(message.message));
        return;
      }

      const result: ConvertResult = {
        ...message.result,
        parts: message.result.parts.map((part) => ({
          ...part,
          bytes: new Uint8Array(part.bytes),
        })),
      };
      resolve(result);
    };

    worker.onerror = (event) => {
      if (!finish()) return;
      signal?.removeEventListener('abort', onAbort);
      reject(new Error(event.message || '转换失败'));
    };

    // Copy the input: the caller may still need its buffer (e.g. re-generate
    // with different options).
    const copy = buffer.slice(0);
    worker.postMessage({ id, buffer: copy, options: rest }, [copy]);
  });
}
