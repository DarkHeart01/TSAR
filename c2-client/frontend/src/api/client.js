import axios from 'axios';
import { API_BASE_URL } from '../utils/constants';

const apiClient = axios.create({
  baseURL: API_BASE_URL,
  timeout: 30000,
  headers: {
    'Content-Type': 'application/json',
  },
});

// Request interceptor for authentication (future implementation)
apiClient.interceptors.request.use(
  (config) => {
    const token = localStorage.getItem('jocky_auth_token');
    if (token) {
      config.headers.Authorization = `Bearer ${token}`;
    }
    return config;
  },
  (error) => Promise.reject(error)
);

// Response interceptor for error handling
apiClient.interceptors.response.use(
  (response) => response.data,
  (error) => {
    if (error.response?.status === 401) {
      // Handle unauthorized access
      console.error('Unauthorized access detected');
    }
    return Promise.reject(error);
  }
);

export const endpointsAPI = {
  getEndpoints: () => apiClient.get('/endpoints'),
};

export const buildAPI = {
  buildPayload: (config) => apiClient.post('/build', config),
};

export const telemetryAPI = {
  getTelemetry: () => apiClient.get('/telemetry'),
};

export default apiClient;