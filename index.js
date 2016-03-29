"use strict";

const stream = require('stream');

function loadBinding() {
	try {
		return require('./build/Release/binding.node');
	} catch(err) {
		return require('./build/Debug/binding.node');
	}
}

module.exports = loadBinding();

module.exports.ForeignAPC = class ForeignAPC extends module.exports.APC {
	constructor(obj) {
		super(obj.threadId, obj.addrOfAPC);
	}
}

Object.defineProperties(Object.getPrototypeOf(Object.getPrototypeOf(module.exports.APC)), {
	toJSON: {value: function() {
		return Object.assign({"type": "ForeignAPC"}, this);
	}}
});

module.exports.ReadableAPC = class ReadableAPC extends stream.Readable {
	constructor(options) {
		super(Object.assign({"objectMode": true}, options));
		this.userAPC = new module.exports.APC((function(num) {
			this.push(num);
		}).bind(this));
	}

	_read(size) {
		//module.exports.DequeueAPCs(); //this part not needed if libuv modified for GetQueuedCompletionStatusEx(fAlertable: TRUE)
	}

	toJSON() {
		return Object.assign({"type": "ReadableAPC"}, this.userAPC);
	}
};

module.exports.WritableAPC = class WritableAPC extends stream.Writable {
	constructor(options) {
		super(Object.assign({"objectMode": true}, options));
		this.userAPC = new module.exports.ForeignAPC({threadId: options.threadId, addrOfAPC: options.addrOfAPC});
	}

	_write(chunk, encoding, callback) {
		this.userAPC.queue(chunk);
		callback();
	}

	_writev(chunks, callback) {
		const chunks_length = chunks.length;
		for(let chunkNum = 0; chunkNum < chunks_length; chunkNum++) {
			this.userAPC.queue(chunks[chunkNum].chunk);
		}
		callback();
	}

	toJSON() {
		return Object.assign({"type": "WritableAPC"}, this.userAPC);
	}

};