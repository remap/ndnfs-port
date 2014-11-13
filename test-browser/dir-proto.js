// protobuf js file for directory, created from dir.proto

var DirProto = dcodeIO.ProtoBuf.newBuilder().import({
    "package": "DirProto",
    "messages": [
        {
            "name": "DirInfo",
            "fields": [
                {
                    "rule": "required",
                    "type": "string",
                    "name": "path",
                    "id": 1,
                    "options": {}
                },
                {
                    "rule": "required",
                    "type": "uint32",
                    "name": "type",
                    "id": 2,
                    "options": {}
                }
            ],
            "enums": [],
            "messages": [],
            "options": {}
        },
        {
            "name": "DirInfoArray",
            "fields": [
                {
                    "rule": "repeated",
                    "type": "DirInfo",
                    "name": "di",
                    "id": 1,
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
}).build("DirProto");
