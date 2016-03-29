"use strict";

const os = require('os');
const apc = require('apc');
const cluster = require('cluster');


if (cluster.isMaster) {
	console.log('apc');
	console.dir(apc, {"showHidden": true, "depth": null, "colors": true});

	os.cpus().forEach(function(cpu, cpuNum) {
		const worker = cluster.fork();
		const masterAPC = new apc.ReadableAPC();
		masterAPC.on('data', function(num) {
			console.log('masterAPC on data', num);
		});
		masterAPC.name = 'masterAPC';

		worker.send(masterAPC);
		worker.on('message', function(message) {
			console.log('master got message', message);
			const workerAPC = new apc.WritableAPC(message);
			workerAPC.write(3);
		});


	});

} else {
	const workerAPC = new apc.ReadableAPC();
	workerAPC.on('data', function(num) {
		console.log('workerAPC on data', num);
	});
	workerAPC.name = 'workerAPC';

	process.send(workerAPC);

	process.on('message', function(message) {
		console.log('worker got message', message);
		const masterAPC = new apc.WritableAPC(message);
		masterAPC.write(5);
	});

}

setTimeout(function() {
	apc.DequeueAPCs(); //this part not needed if libuv modified for GetQueuedCompletionStatusEx(fAlertable: TRUE)
}, 1000);
