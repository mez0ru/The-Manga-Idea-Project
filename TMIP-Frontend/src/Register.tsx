import React, { useEffect, useRef, useState } from 'react'
import Grid from '@mui/material/Grid';
import Button from '@mui/material/Button';
import TextField from '@mui/material/TextField';
import Typography from '@mui/material/Typography';
import Alert from '@mui/material/Alert';
import axios from "./api/axios";
import { AxiosError } from 'axios';
import { useNavigate } from 'react-router-dom';

const EMAIL_REGEX = /^[\w-\.]+@([\w-]+\.)+[\w-]{2,4}$/;
const PWD_REGEX = /^.{4,}/;
const REGISTER_URL = '/api/register'

export default function Register() {

    const emailRef = useRef<HTMLDivElement>(null);
    const errRef = useRef<HTMLDivElement>(null);

    const [email, setEmail] = useState('');
    const [emailFocus, setEmailFocus] = useState(false);

    const [pwd, setPwd] = useState('');
    const [validEmail, setValidEmail] = useState(false);
    const [validPwd, setValidPwd] = useState(false);
    const [pwdFocus, setPwdFocus] = useState(false);

    const [matchPwd, setMatchPwd] = useState('');
    const [validMatch, setValidMatch] = useState(false);
    const [matchFocus, setMatchFocus] = useState(false);

    const [errMsg, setErrMsg] = useState('');
    const [success, setSuccess] = useState(false);

    const navigate = useNavigate();

    useEffect(() => {
        const result = EMAIL_REGEX.test(email);
        console.log(result);
        console.log(email);
        setValidEmail(result);
    }, [email])

    useEffect(() => {
        const result = PWD_REGEX.test(pwd);
        console.log(result);
        console.log(pwd);
        setValidPwd(result);
        setValidMatch(pwd === matchPwd);
    }, [pwd, matchPwd])

    useEffect(() => {
        setErrMsg('');
    }, [email, pwd, matchPwd])

    const handleSubmit = async (e) => {
        e.preventDefault();

        // if button enabled with JS hack
        const v1 = EMAIL_REGEX.test(email);
        const v2 = PWD_REGEX.test(pwd);
        if (!v1 || !v2) {
            setErrMsg("Invalid Entry");
            return;
        }
        try {
            const response = await axios.post(REGISTER_URL,
                JSON.stringify({ email, password: pwd }),
                {
                    headers: { 'Content-Type': 'application/json' },
                    withCredentials: true
                }) as any



            console.log(response.data ?? "No Data");
            console.log(response.data.accessToken ?? "No Access token");
            console.log(JSON.stringify(response));
            setSuccess(true);

            if (response.data?.success) {
                navigate('/home');
            }

            // clear input fields
        } catch (err: unknown) {
            if (err instanceof AxiosError) {
                if (!err?.response) {
                    setErrMsg('No Server Response');
                } else if (err.response?.status === 409) {
                    setErrMsg('Email Taken');
                } else {
                    setErrMsg(err?.response.data?.error);
                }

                console.error(err)
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
                        Register
                    </Typography>
                    <Alert severity="error" ref={errRef} style={{ display: `${errMsg ? 'flex' : 'none'}` }}>{errMsg}</Alert>
                    <Grid item>
                        <TextField
                            required
                            id="outlined-required"
                            label="Email"
                            ref={emailRef}
                            type="email"
                            style={{ width: '20rem' }}
                            autoFocus
                            onChange={(e) => setEmail(e.target.value)}
                            value={email}
                            aria-invalid={validEmail ? "false" : "true"}
                            aria-describedby="uidnote"
                            onFocus={() => setEmailFocus(true)}
                            onBlur={() => setEmailFocus(false)}
                        /></Grid>
                    <Grid item>
                        <TextField
                            required
                            id="outlined-required"
                            label="Password"
                            type="password"
                            style={{ width: '20rem' }}
                            onChange={(e) => setPwd(e.target.value)}
                            value={pwd}
                            aria-invalid={validPwd ? "false" : "true"}
                            aria-describedby="pwdnote"
                            onFocus={() => setPwdFocus(true)}
                            onBlur={() => setPwdFocus(false)}
                        /></Grid>
                    <Grid item>
                        <TextField
                            required
                            id="outlined-required"
                            label="Confirm Password"
                            type="password"
                            style={{ width: '20rem' }}
                            onChange={(e) => setMatchPwd(e.target.value)}
                            value={matchPwd}
                            aria-invalid={validMatch ? "false" : "true"}
                            aria-describedby="confirmnote"
                            onFocus={() => setMatchFocus(true)}
                            onBlur={() => setMatchFocus(false)}
                        /></Grid>
                    <Grid item>
                        <Button type='submit' variant="contained" disabled={!validEmail || !validPwd || !validMatch ? true : false}>Register</Button>
                    </Grid>
                </Grid>
            </form>
        </Grid>

    )
}
