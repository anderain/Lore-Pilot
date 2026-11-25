// Progress reporting for the text -> PDB pipeline.
//
// The pipeline runs inside a Web Worker for large books, so progress has to
// cross a message boundary. Stages are coarse on purpose: the UI only needs
// "which phase" and "how far".

export type ConvertPhase = 'decode' | 'layout' | 'pack';

export interface ConvertProgress {
  phase: ConvertPhase;
  /** Completed units within the phase. */
  done: number;
  /** Total units in the phase. */
  total: number;
  /** 0..100 over the whole pipeline. */
  percent: number;
}

export type ProgressCallback = (progress: ConvertProgress) => void;

/** Weight of each phase in the overall percentage.
 *  Measured on a 4 MB book: decode ~4%, layout ~16%, pack ~63%. */
const PHASE_WEIGHT: Record<ConvertPhase, number> = {
  decode: 5,
  layout: 20,
  pack: 75,
};

const PHASE_BASE: Record<ConvertPhase, number> = {
  decode: 0,
  layout: 5,
  pack: 25,
};

export function phasePercent(
  phase: ConvertPhase,
  done: number,
  total: number,
): number {
  const safeTotal = total > 0 ? total : 1;
  const ratio = Math.max(0, Math.min(1, done / safeTotal));
  return Math.min(100, Math.round(PHASE_BASE[phase] + PHASE_WEIGHT[phase] * ratio));
}

export function progressLabel(phase: ConvertPhase): string {
  switch (phase) {
    case 'decode':
      return '解码';
    case 'layout':
      return '排版';
    default:
      return '打包';
  }
}
