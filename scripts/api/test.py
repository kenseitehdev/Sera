#!/usr/bin/env python3
# Sera test endpoint
import json

data = {
    'status': 'ok',
    'message': 'Sera server is running',
    'server': 'Sera/1.0'
}

body = json.dumps(data)
response = f'HTTP/1.1 200 OK
Content-Type: application/json

{body}'
