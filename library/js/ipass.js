(function () {
    if (Module['Pipeline']) {
        return;
    }

    function Pipeline(gpuDevice, options) {
        options = options || {};
        this._inner = new Module._Pipeline(gpuDevice, options.maxPatches || 128);
    }

    Pipeline.prototype.loadPatches = function (patchData) {
        return this._inner.loadPatches(
            patchData.controlPoints,
            patchData.cornerIndices,
            patchData.numPatches
        );
    };

    Pipeline.prototype.setMVP = function (matrix) {
        var f32 = (matrix instanceof Float32Array) ? matrix : new Float32Array(matrix);
        this._inner.setMVP(f32);
    };

    Pipeline.prototype.setViewport = function (width, height) {
        this._inner.setViewport(width, height);
    };

    Pipeline.prototype.execute = function (commandEncoder) {
        return this._inner.execute(commandEncoder);
    };

    Pipeline.prototype.dispatchLOD = function (commandEncoder) {
        return this._inner.dispatchLOD(commandEncoder);
    };

    Pipeline.prototype.dispatchTessellation = function (commandEncoder) {
        return this._inner.dispatchTessellation(commandEncoder);
    };

    Pipeline.prototype.getVertexBuffer = function () {
        return this._inner.getVertexBuffer();
    };

    Pipeline.prototype.getLODBuffer = function () {
        return this._inner.getLODBuffer();
    };

    Pipeline.prototype.getMaxVertexCount = function () {
        return this._inner.getMaxVertexCount();
    };

    Pipeline.prototype.destroy = function () {
        if (this._inner) {
            this._inner.delete();
            this._inner = null;
        }
    };


    function loadBVFile(arrayBuffer, options) {
        options = options || {};
        var maxPatches = options.maxPatches || 0;
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
            numPatches: result.numPatches
        };
    }

    var Status = {
        Success:          Number(Module.Status.Success),
        EmptyInput:       Number(Module.Status.EmptyInput),
        GPUInitFailed:    Number(Module.Status.GPUInitFailed),
        NotInitialized:   Number(Module.Status.NotInitialized),
        PatchesNotLoaded: Number(Module.Status.PatchesNotLoaded),
        LoadFailed:       Number(Module.Status.LoadFailed)
    };

    Module['Pipeline'] = Pipeline;
    Module['loadBVFile'] = loadBVFile;
    Module['Status'] = Status;
    Module['OutputVertexLayout'] = {
        FLOATS_PER_VERTEX: Module.FLOATS_PER_VERTEX,
        BYTES_PER_VERTEX: Module.BYTES_PER_VERTEX,
        POSITION_OFFSET: Module.POSITION_OFFSET,
        NORMAL_OFFSET: Module.NORMAL_OFFSET,
        COLOR_OFFSET: Module.COLOR_OFFSET,
        UV_OFFSET: Module.UV_OFFSET
    };
})();
