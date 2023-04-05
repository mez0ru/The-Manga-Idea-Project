import { useEffect, useState } from 'react'
import { useLocation, useNavigate } from 'react-router-dom';
import useAxiosPrivate from './hooks/useAxiosPrivate';
import Grid from '@mui/material/Grid';
import SeriesCard from './SeriesCard';

export interface Series {
    id: number;
    name: string;
}

export default function Home() {
    const navigate = useNavigate();
    const location = useLocation();
    const axiosPrivate = useAxiosPrivate();

    const [series, setSeries] = useState<Series[]>([])



    useEffect(() => {
        let isMounted = true;
        const controller = new AbortController();

        const getSeries = async () => {
            try {
                const response = await axiosPrivate.get('/api/series/info', {
                    signal: controller.signal
                });
                console.log(response.data);
                isMounted && setSeries(response.data as Series[]);
            } catch (err) {
                console.error(err);
                // navigate('/login', { state: { from: location }, replace: true });
            }
        }

        getSeries();

        return () => {
            isMounted = false;
            controller.abort();
        }
    }, [])

    return (<div>
        <Grid container spacing={2} direction="row">
            {
                series.map((item, i) => (
                    <Grid item key={i}>
                        <SeriesCard series={item} />
                    </Grid>))
            }
        </Grid>
    </div>)
}
