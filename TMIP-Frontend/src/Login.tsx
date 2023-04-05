import React from 'react'
import Grid from '@mui/material/Grid';
import Button from '@mui/material/Button';
import TextField from '@mui/material/TextField';
import Typography from '@mui/material/Typography';

export default function Login() {
    return (
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
            </Typography>;
            <Grid item>
                <TextField
                    required
                    id="outlined-required"
                    label="Email"
                    type="email"
                    style={{ width: '24rem' }}
                    autoFocus
                /></Grid>
            <Grid item>
                <TextField
                    required
                    id="outlined-required"
                    label="Password"
                    type="password"
                    style={{ width: '24rem' }}
                /></Grid>
            <Grid item>
                <Button variant="contained">Login</Button>
            </Grid>
        </Grid>
    )
}
