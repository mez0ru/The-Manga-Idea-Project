import { useRef, useState, useEffect } from 'react'
import Grid from '@mui/material/Grid';
import Button from '@mui/material/Button';
import TextField from '@mui/material/TextField';
import Typography from '@mui/material/Typography';
import Alert from '@mui/material/Alert';
import useAuth from './hooks/useAuth';
import { useLocation, useNavigate } from 'react-router-dom';
import axios from "./api/axios";
import { AxiosError } from 'axios';

const LOGIN_URL = '/api/login';

export default function Login() {
    const { setAuth } = useAuth();

    const navigate = useNavigate();
    const location = useLocation();
    const from = location.state?.from?.pathname || "/";

    const emailRef = useRef<HTMLDivElement>(null);
    const errRef = useRef<HTMLDivElement>(null);

    const [email, setEmail] = useState('');
    const [pwd, setPwd] = useState('');
    const [errMsg, setErrMsg] = useState('');

    useEffect(() => {
        emailRef.current?.focus();
    }, [])

    useEffect(() => {
        setErrMsg('');
    }, [email, pwd])

    const handleSubmit = async (e) => {
        e.preventDefault();

        try {
            const response = await axios.post(LOGIN_URL,
                JSON.stringify({ email, password: pwd }),
                {
                    headers: { 'Content-Type': 'application/json' },
                    withCredentials: true
                }
            );
            console.log(JSON.stringify(response?.data))
            // console.log(JSON.stringify(response))
            const accessToken = response?.data?.accessToken;
            const roles = response?.data?.roles;
            // setAuth({ email, pwd, roles, accessToken })
            setAuth!!({ accessToken })
            setEmail('');
            setPwd('');
            navigate(from, { replace: true });
        } catch (err) {
            if (err instanceof AxiosError) {
                if (!err?.response) {
                    setErrMsg('No Server Response');
                } else if (err.response?.status === 400) {
                    setErrMsg('Missing Email or Password');
                } else if (err.response?.status === 401) {
                    setErrMsg('Unauthorized');
                } else {
                    setErrMsg('Login Failed');
                }
                errRef.current?.focus();
            }
        }
    }

    return (
        <Grid
            container
            justifyContent="center"
        >
            <form onSubmit={handleSubmit}>
                <Grid
                    container
                    direction="column"
                    justifyContent="center"
                    alignItems="center"
                    style={{ minHeight: '84vh' }}
                    spacing={3}
                >
                    <Typography variant="h2" component="h2">
                        Login
                    </Typography>
                    <Alert severity="error" ref={errRef} style={{ display: `${errMsg ? 'flex' : 'none'}` }}>{errMsg}</Alert>
                    <Grid item>
                        <TextField
                            required
                            id="outlined-required"
                            label="Email"
                            type="email"
                            style={{ width: '24rem' }}
                            autoFocus
                            onChange={(e) => setEmail(e.target.value)}
                            value={email}
                            ref={emailRef}
                        /></Grid>
                    <Grid item>
                        <TextField
                            required
                            id="outlined-required"
                            label="Password"
                            type="password"
                            style={{ width: '24rem' }}
                            onChange={(e) => setPwd(e.target.value)}
                            value={pwd}
                        /></Grid>
                    <Grid item>
                        <Button type='submit' variant="contained">Login</Button>
                    </Grid>
                </Grid>
            </form>
        </Grid>
    )
}
