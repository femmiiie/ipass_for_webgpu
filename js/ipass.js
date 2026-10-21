(function () {
    if (Module['ComputePipeline'] || Module['VertexPipeline']) {
        return;
    }

    // maxPatches = 0 autosizes based on device limits
    function ComputePipeline(gpuDevice, maxPatches) {
        this._inner = new Module._GeometryPipeline(gpuDevice, maxPatches || 0);
    }

    function VertexPipeline(gpuDevice, maxPatches) {
        this._inner = new Module._DirectPipeline(gpuDevice, maxPatches || 0);
    }

    // methods both pipelines share
    function addSharedMethods(proto) {
        proto.loadPatches = function (patchData) {
            return this._inner.loadPatches(
                patchData.controlPoints,
                patchData.cornerIndices,
                patchData.numPatches
            );
        };

        proto.setMVP = function (matrix) {
            var f32 = (matrix instanceof Float32Array) ? matrix : new Float32Array(matrix);
            this._inner.setMVP(f32);
        };

        proto.setViewport = function (width, height) {
            this._inner.setViewport(width, height);
        };

        proto.execute = function (commandEncoder) {
            return this._inner.execute(commandEncoder);
        };

        proto.dispatchLOD = function (commandEncoder) {
            return this._inner.dispatchLOD(commandEncoder);
        };

        proto.dispatchTessellation = function (commandEncoder) {
            return this._inner.dispatchTessellation(commandEncoder);
        };

        proto.getLODBuffer = function () {
            return this._inner.getLODBuffer();
        };

        proto.destroy = function () {
            if (this._inner) {
                this._inner.delete();
                this._inner = null;
            }
        };
    }

    addSharedMethods(ComputePipeline.prototype);
    addSharedMethods(VertexPipeline.prototype);

    ComputePipeline.prototype.getVertexBuffer = function () {
        return this._inner.getVertexBuffer();
    };

    ComputePipeline.prototype.getMaxVertexCount = function () {
        return this._inner.getMaxVertexCount();
    };

    VertexPipeline.prototype.draw = function (renderPassEncoder) {
        return this._inner.draw(renderPassEncoder);
    };

    VertexPipeline.prototype.getSurfaceBindGroupLayout = function () {
        return this._inner.getSurfaceBindGroupLayout();
    };

    VertexPipeline.getSurfaceShaderSource = function () {
        return Module._DirectPipeline.getSurfaceShaderSource();
    };


    // maxPatches = 0 loads every patch
    function loadBVFile(arrayBuffer, maxPatches) {
        maxPatches = maxPatches || 0;
        var filename = '/tmp/ipass_' + Date.now() + '.bv';

        // write to virtual FS
        var data = new Uint8Array(arrayBuffer);
        Module.FS.writeFile(filename, data);

        var result = Module._loadBVFile(filename, maxPatches);

        Module.FS.unlink(filename);

        return {
            status: result.status,
            controlPoints: result.controlPoints,
            cornerIndices: result.cornerIndices,
            numPatches: result.numPatches,
            patchData: function() {
                return {
                    controlPoints: this.controlPoints,
                    cornerIndices: this.cornerIndices,
                    numPatches: this.numPatches
                };
            }
        };
    }

    var Status = {
        Success: Number(Module.Status.Success),
        EmptyInput: Number(Module.Status.EmptyInput),
        GPUInitFailed: Number(Module.Status.GPUInitFailed),
        NotInitialized: Number(Module.Status.NotInitialized),
        PatchesNotLoaded: Number(Module.Status.PatchesNotLoaded),
        LoadFailed: Number(Module.Status.LoadFailed)
    };

    Module['ComputePipeline'] = ComputePipeline;
    Module['VertexPipeline'] = VertexPipeline;
    Module['loadBVFile'] = loadBVFile;
    Module['Status'] = Status;
    Module['SURFACE_BIND_GROUP'] = Module.SURFACE_BIND_GROUP;
    Module['OutputVertexLayout'] = {
        FLOATS_PER_VERTEX: Module.FLOATS_PER_VERTEX,
        BYTES_PER_VERTEX: Module.BYTES_PER_VERTEX,
        POSITION_OFFSET: Module.POSITION_OFFSET,
        NORMAL_OFFSET: Module.NORMAL_OFFSET,
        COLOR_OFFSET: Module.COLOR_OFFSET,
        UV_OFFSET: Module.UV_OFFSET
    };
})();
