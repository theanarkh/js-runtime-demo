const {
    socket,
    console,
} = Deer.buildin;
const { events } = Deer.libs;
class Server extends events {
    socket = null;
    connections = 0;
    constructor() {
        super();
    }
    listen(options = {}) {
        this.socket = new socket.Socket();  
        this.socket.bind(options.host, options.port);
        return this.socket.listen(512, (socket) => {
            const s = new Socket({socket});
            s.read();
            this.connections++;
            this.emit('connection', s);
        });
    }
}

class Socket extends events {
    constructor(options = {}) {
        super();
        this.socket = options.socket || new socket.Socket();
    }
    read() {
        this.socket.read((bytes, data) => {
            if (bytes > 0) {
                this.emit('data', data, bytes);
            }
        });
    }
    write(data, cb) {
        if (typeof data === 'string') {
            data = Buffer.from(data);
        }
        const writeRequest = new socket.WriteRequest();
        writeRequest.oncomplete = () => {
            cb && cb();
        };
        this.socket.write(data, writeRequest);
    }
}

function createServer(...arg) {
    return new Server(...arg);
}

module.exports = {
    createServer,
    Server,
    Socket,
}