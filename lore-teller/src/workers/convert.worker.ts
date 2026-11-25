/// <reference lib="webworker" />
// Worker entry: runs the whole text -> PDB pipeline off the main thread.
//
// Large books (tens of MB) used to freeze the page for seconds. Moving the
// work here keeps the spinner animating and the UI responsive.

import { convertText } from '../lib/convert';
import type { ConvertResult } from '../lib/convert';
import type { ConvertProgress } from '../lib/progress';
import type { WorkerRequest, WorkerResponse } from '../lib/convertWorkerProtocol';
import { resultTransferables } from '../lib/convertWorkerProtocol';

type ResultWithBuffers = Extract<WorkerResponse, { type: 'done' }>['result'];

function toTransferable(result: ConvertResult): ResultWithBuffers {
  return {
    ...result,
    parts: result.parts.map((part) => ({
      ...part,
      bytes: part.bytes.buffer.slice(
        part.bytes.byteOffset,
        part.bytes.byteOffset + part.bytes.byteLength,
      ) as ArrayBuffer,
    })),
  };
}

self.onmessage = (event: MessageEvent<WorkerRequest>) => {
  const { id, buffer, options } = event.data;

  const post = (message: WorkerResponse, transfer?: Transferable[]) => {
    if (transfer === undefined) {
      self.postMessage(message);
    } else {
      self.postMessage(message, transfer);
    }
  };

  try {
    const result = convertText(buffer, {
      ...options,
      onProgress: (progress: ConvertProgress) => {
        post({ id, type: 'progress', progress });
      },
    });
    const payload = toTransferable(result);
    post({ id, type: 'done', result: payload }, resultTransferables(payload));
  } catch (error) {
    post({
      id,
      type: 'error',
      message: error instanceof Error ? error.message : String(error),
    });
  }
};
