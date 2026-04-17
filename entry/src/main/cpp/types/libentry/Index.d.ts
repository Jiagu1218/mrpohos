export declare const init: (sandboxPath: string) => number;
export declare const start: (filename: string, extName: string) => number;
export declare const event: (code: number, p0: number, p1: number) => number;
export declare const timer: () => number;
export declare const onTimerStart: (callback: (t: number) => void) => void;
export declare const onTimerStop: (callback: () => void) => void;
export declare const onEditRequest: (callback: (title: string, text: string, type: number, maxSize: number) => void) => void;
export declare const setEditResult: (text: string) => void;
