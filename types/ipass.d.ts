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
    patchData(): PatchData;
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

// methods both pipelines share
declare abstract class PipelineBase {
    loadPatches(data: PatchData): number;
    setMVP(matrix: Float32Array | number[]): void;
    setViewport(width: number, height: number): void;

    execute(encoder: GPUCommandEncoder): number;
    dispatchLOD(encoder: GPUCommandEncoder): number;
    dispatchTessellation(encoder: GPUCommandEncoder): number;

    getLODBuffer(): GPUBuffer;

    destroy(): void;
}

// Tessellates into a vertex buffer, see OutputVertexLayout.
export declare class ComputePipeline extends PipelineBase {
    constructor(device: GPUDevice, maxPatches?: number); // 0 autosizes based on device limits

    getVertexBuffer(): GPUBuffer;
    getMaxVertexCount(): number;
}

// Evaluates the surface in your vertex shader, see draw.
export declare class VertexPipeline extends PipelineBase {
    constructor(device: GPUDevice, maxPatches?: number); // 0 autosizes based on device limits

    draw(pass: GPURenderPassEncoder): number;
    getSurfaceBindGroupLayout(): GPUBindGroupLayout;
    static getSurfaceShaderSource(): string;
}

export interface IPASSModule {
    ComputePipeline: typeof ComputePipeline;
    VertexPipeline: typeof VertexPipeline;
    OutputVertexLayout: OutputVertexLayout;
    Status: typeof Status;
    SURFACE_BIND_GROUP: 1;

    loadBVFile(data: ArrayBuffer, maxPatches?: number): BVLoadResult; // 0 loads every patch
}

declare function createIPASS(): Promise<IPASSModule>;
export default createIPASS;
