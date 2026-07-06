FROM python:3.12-slim

# Prevent Python from writing .pyc files
ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1

WORKDIR /app

# Install dependencies
COPY web/backend/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy application and engines
COPY web/backend/ .
COPY engines/ /engines/

# Expose the port (Render ignores this but it's good practice)
EXPOSE 8000

# Start FastAPI
CMD ["sh", "-c", "uvicorn main:app --host 0.0.0.0 --port ${PORT:-8000}"]
