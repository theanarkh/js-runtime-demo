const {
    console,
} = Deer.buildin;
const server = Deer.libs.tcp.createServer();

const ret = server.listen({host: '127.0.0.1', port: 8888});
if (ret) {
    console.log('listen error\n');
} else {
    console.log('listen successfully\n');
    server.on('connection', (socket) => {
        console.log('receive connection\n');
        socket.on('data', (data, len) => {
            console.log(`receive data: len: ${len}, data: ${Buffer.toString(data)}\n`);
        });
        socket.write("hello", () => {
            console.log('write done\n');
        });
    });
}