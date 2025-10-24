#!/usr/bin/env python3
# Example with dynamic routing
import json

# Get route parameter
params = environ.get('route.params', {})
user_id = params.get('id', 'unknown')

data = {
    'id': user_id,
    'name': f'User {user_id}',
    'email': f'user{user_id}@example.com'
}

body = json.dumps(data)
response = f'HTTP/1.1 200 OK
Content-Type: application/json

{body}'
