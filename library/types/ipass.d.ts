export interface PipelineOptions {
    maxPatches?: number;
}

export interface PatchData {
    controlPoints: Float32Array;
    cornerIndices: Uint32Array;
    numPatches: number;
}

export interface BVLoadResult {
    status: number;
    controlPoints: Float32Array;
    cornerIndices: Uint32Array;
    numPatches: number;
}

export interface OutputVertexLayout {
    FLOATS_PER_VERTEX: 16;
    BYTES_PER_VERTEX: 64;
    POSITION_OFFSET: 0;
    NORMAL_OFFSET: 16;
    COLOR_OFFSET: 32;
    UV_OFFSET: 48;
}

export declare enum Status {
    Success = 0,
    EmptyInput = 1,
    GPUInitFailed = 2,
    NotInitialized = 3,
    PatchesNotLoaded = 4,
    LoadFailed = 5,
}

export declare class Pipeline {
    constructor(device: GPUDevice, options?: PipelineOptions);

    loadPatches(data: PatchData): number;
    setMVP(matrix: Float32Array | number[]): void;
    setViewport(width: number, height: number): void;

    execute(encoder: GPUCommandEncoder): number;
    dispatchLOD(encoder: GPUCommandEncoder): number;
    dispatchTessellation(encoder: GPUCommandEncoder): number;

    getVertexBuffer(): GPUBuffer;
    getLODBuffer(): GPUBuffer;
    getMaxVertexCount(): number;

    destroy(): void;
}

export interface IPASSModule {
    Pipeline: typeof Pipeline;
    OutputVertexLayout: OutputVertexLayout;
    Status: typeof Status;

    loadBVFile(data: ArrayBuffer, options?: { maxPatches?: number }): BVLoadResult;
}

declare function createIPASS(): Promise<IPASSModule>;
export default createIPASS;
