import { useEffect, useState } from 'react'
import { useLocation, useNavigate, useOutletContext } from 'react-router-dom';
import useAxiosPrivate from './hooks/useAxiosPrivate';
import Grid from '@mui/material/Grid';
import SeriesCard from './Components/SeriesCard';
import { useMyOutletContext } from './Dashboard';
import { AxiosError } from 'axios';

export interface Series {
    id: number;
    name: string;
    chapters_count: number;
}

export default function Home() {
    const { invalidate, setInvalidate } = useMyOutletContext();
    const navigate = useNavigate();
    const location = useLocation();
    const axiosPrivate = useAxiosPrivate();

    const [series, setSeries] = useState<Series[]>([])

    useEffect(() => {
        let isMounted = true;
        const controller = new AbortController();

        const getSeries = async () => {
            try {
                const response = await axiosPrivate.get('/api/v2/series', {
                    signal: controller.signal,
                });
                console.log(response.data);
                isMounted && setSeries(response.data as Series[]);
                setInvalidate(false);
            } catch (err) {
                if (err instanceof AxiosError) {
                    console.error(err);
                    if (err.response?.status === 401 || err.response?.status === 403)
                        navigate('/login', { state: { from: location }, replace: true });
                }
            }
        }

        getSeries();

        return () => {
            isMounted = false;
            controller.abort();
        }
    }, [invalidate])

    return (<div>
        <Grid container px={{ xs: 3, sm: 2, md: 2, lg: 2 }} spacing={2} direction="row">
            {
                series.map((item, i) => (
                    <Grid item key={i}>
                        <SeriesCard series={item} />
                    </Grid>))
            }
        </Grid>
    </div>)
}
