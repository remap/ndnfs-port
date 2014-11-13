// protobuf js file for directory, created from file.proto

var FileProto = dcodeIO.ProtoBuf.newBuilder().import({
    "package": "FileProto",
    "messages": [
        {
            "name": "FileInfo",
            "fields": [
                {
                    "rule": "required",
                    "type": "uint32",
                    "name": "size",
                    "id": 1,
                    "options": {}
                },
                {
                    "rule": "required",
                    "type": "uint32",
                    "name": "totalseg",
                    "id": 2,
                    "options": {}
                },
                {
                    "rule": "required",
                    "type": "uint32",
                    "name": "version",
                    "id": 3,
                    "options": {}
                }
            ],
            "enums": [],
            "messages": [],
            "options": {}
        }
    ],
    "enums": [],
    "imports": [],
    "options": {}
}).build("FileProto")
