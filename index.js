"use strict";

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
	}}
});