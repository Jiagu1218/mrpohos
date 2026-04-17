export const init: (sandboxPath: string) => number;
export const start: (filename: string, extName: string) => number;
export const event: (code: number, p0: number, p1: number) => number;
export const timer: () => number;
export const onTimerStart: (callback: (t: number) => void) => void;
export const onTimerStop: (callback: () => void) => void;
