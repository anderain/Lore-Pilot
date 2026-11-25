import type { ConvertOptions, ConvertResult } from './convert';
import type { ConvertProgress } from './progress';

export interface WorkerRequest {
  id: number;
  buffer: ArrayBuffer;
  options: Omit<ConvertOptions, 'onProgress'>;
}

export type WorkerResponse =
  | { id: number; type: 'progress'; progress: ConvertProgress }
  | {
      id: number;
      type: 'done';
      result: Omit<ConvertResult, 'parts'> & {
        parts: Array<Omit<ConvertResult['parts'][number], 'bytes'> & { bytes: ArrayBuffer }>;
      };
    }
  | { id: number; type: 'error'; message: string };

/** Transfer list for a finished result, so the buffers move without a copy. */
export function resultTransferables(
  result: Extract<WorkerResponse, { type: 'done' }>['result'],
): Transferable[] {
  return result.parts.map((part) => part.bytes);
}
