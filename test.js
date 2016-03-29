"use strict";

const os = require('os');
const apc = require('apc');
const cluster = require('cluster');


if (cluster.isMaster) {
	console.log('apc');
	console.dir(apc, {"showHidden": true, "depth": null, "colors": true});

	os.cpus().forEach(function(cpu, cpuNum) {
		const worker = cluster.fork();
		const masterAPC = new apc.APC(function(num) {
			console.log('masterAPC called', cpuNum, num);
		});
		masterAPC.name = 'masterAPC';

		worker.send(masterAPC);
		worker.on('message', function(message) {
			console.log('master got message', message);
			const workerAPC = new apc.ForeignAPC(message);
			console.log('workerAPC: ', workerAPC);
			workerAPC.queue(3);
		});


	});

} else {
	const workerAPC = new apc.APC(function(num) {
		console.log('workerAPC called', num);
	});
	workerAPC.name = 'workerAPC';

	process.send(workerAPC);

	process.on('message', function(message) {
		console.log('worker got message', message);
		const masterAPC = new apc.ForeignAPC(message);
		console.log('masterAPC: ', masterAPC);
		masterAPC.queue(2);
	})
}

//setTimeout(function() {
//	apc.DequeueAPCs(); //this part not needed if libuv modified for GetQueuedCompletionStatusEx(fAlertable: TRUE)
//}, 1000);